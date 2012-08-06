#!/bin/bash
# path to copyq binary
COPYQ=${1:-copyq}
# command to read clipboard
READ=${2:-"xclip -o"}
WRITE=${3:-"xclip"}

ERR=0
ERRMSG=""
T=0
LASTPL=0
TMP="$0.log"
EXIT=0

TAB=test

> "$TMP"

log () {
    echo -ne "\033[0;33m"
    printf "%-50s" "* $@"
    echo -ne "\033[0m"
    echo -e "\n\n* $@" >> "$TMP"
}

ok () {
    if [ $EXIT -eq 0 ]; then
        echo -e "\033[0;32m OK $@\033[0m"
        echo "** OK" >> "$TMP"
    else
        fail
    fi
}

fail () {
    ERR=$((ERR+1))
    echo -ne "\033[0;31m FAILED! $ERRORMSG $@\033[0m"
    [ $EXIT -eq 0 ] && echo || echo "(exit code $EXIT)"
    EXIT=0
    echo "** FAILED! $ERRORMSG $@" >> "$TMP"
    return 1
}

c () {
    args=${TAB:+tab $TAB}
    echo "*** $COPYQ $args $@" >> "$TMP"
    OUT=`"$COPYQ" $args "$@" 2>> "$TMP"`
    EXIT=$?
    [ $EXIT -eq 0 ] || echo "** exit code $EXIT" >> "$TMP"
    [ $EXIT -eq 0 ] || return $EXIT
    printf "%s" "$OUT"
}

perf () {
    T=`{ time -p "$COPYQ" tab "$TAB" "$@" >/dev/null || exit 1; } 2>&1|awk '{print $2;exit}'` || return 1
}

testpl () {
    echo "exit(!($@))" | perl
    LASTPL=$?
    return $LASTPL
}

lastpl () {
    return $LASTPL
}

assert () {
    if [ "$1" != "$2" ]; then
        ERRORMSG="$3 (\"$1\" != \"$2\")"
        return 1
    fi
}

go () {
    "$COPYQ" 2>> "$TMP" &
    sleep 0.5
    pidof -s copyq 2>&1 > /dev/null
}

terminate () {
    "$COPYQ" exit 2>&1 > /dev/null
    while pidof -s copyq 2>&1 > /dev/null; do
        sleep 1
    done
    return 0
}

restart ()
{
    log "restarting copyq server"
    { terminate && go; } 2>&1 > /dev/null &&
        ok || fail
}

clipboard ()
{
    sleep 0.5
    $READ
}

ulimit -c unlimited

restart || exit 1

log "add items"
c add 3 2 1 0 &&
    assert "0" "`c read 0`" "first item" &&
    assert "0" "`clipboard`" "first item in clipboard" &&
    assert "1" "`c read 1`" "second item" &&
    assert "2" "`c read 2`" "third item" &&
    assert "3" "`c read 3`" "fourth item" &&
    ok || fail

log "remove items"
c remove &&
    assert "1" "`c read 0`" "first item after first removal" &&
    assert "1" "`clipboard`" "clipboard after first removal" &&
c remove &&
    assert "2" "`c read 0`" "first item after second removal" &&
    assert "2" "`clipboard`" "clipboard after second removal" &&
    ok || fail

log "move second item to clipboard"
i1=`c read 1`
c "select" 1
assert "$i1" "`c read 0`" && assert "$i1" "`clipboard`" &&
    ok || fail

log "clipboard content"
for x in 1 2 3; do echo "$x"|xclip; done && sleep 0.25 &&
    assert "3" "`c read`" && c remove &&
for x in 4 5 6; do echo "$x"|xclip; done && sleep 0.25 &&
    assert "6" "`c read`" && c remove &&
    ok || fail

log "read past end of list"
SIZE=`c size` &&
    X=`c read $SIZE` &&
    assert "$X" "" &&
    ok || fail

log "time - \"copyq size\" in 0.1 seconds"
{ for _ in {1..10}; do
    perf size &&
    testpl "$T <= 0.1" || break
done } && lastpl &&
    ok || fail "($T seconds)"

log "time - \"copyq select 1\" in 0.3 seconds"
{ for _ in {1..10}; do
    perf "select" 1 &&
    testpl "$T <= 0.3" || break
done } && lastpl &&
    ok || fail "($T seconds)"

log "adding 30 items (each in 0.1 seconds)"
str=""
{ for x in {1..30}; do
    perf add "$x" &&
    str="$x "$str
    testpl "$T <= 0.1" || break
done } && lastpl &&
    assert "`c separator ' ' read {0..29}` " "$str" &&
    ok || fail "($T seconds)"

log "removing 30 items (each in 0.1 seconds)"
{ for _ in {0..29}; do
    perf remove 0 &&
    testpl "$T <= 0.1" || break
done } && lastpl &&
    ok || fail "($T seconds)"

log "adding 100 items at once in 0.3 seconds"
perf add {1..100} && testpl "$T <= 0.3" &&
    ok || fail "($T seconds)"

log "reading mime of 100 items at once in 0.1 seconds"
perf read "?" {1..100} && testpl "$T <= 0.1" &&
    ok || fail "($T seconds)"

log "removing 100 items at once in 0.3 seconds"
perf remove {0..99} && testpl "$T <= 0.3" &&
    ok || fail "($T seconds)"

mime="application/copyq-test"
log "adding huge amount of data in 1.0 seconds"
echo {0..99999}|perf write "$mime" - && testpl "$T <= 1.0" &&
    ok || fail "($T seconds)"

log "reading huge amount of data"
[ "`c read "$mime" 0`" = "`echo {0..99999}`" ] &&
    ok || fail

mime="text/plain"
log "adding huge amount of text in 1.0 seconds"
echo {0..99999}|perf write "$mime" - && testpl "$T <= 1.0" &&
    ok || fail "($T seconds)"

log "reading huge amount of text"
[ "`c read "$mime" 0`" = "`echo {0..99999}`" ] &&
    ok || fail

log "reading huge amount of text from clipboard"
[ "`clipboard`" = "`echo {0..99999}`" ] &&
    ok || fail

log "removing huge amount of data in 0.1 seconds"
perf remove 0 && testpl "$T < 0.1" &&
    ok || fail "($T seconds)"

TAB=test2
log "creating items in a new tab"
perf add 1 2 3 && testpl "$T < 0.1" &&
    ok || fail "($T seconds)"

restart || exit 1

log "checking items in the new tab"
    assert "3" "`c read 0`" "first item" &&
    assert "2" "`c read 1`" "second item" &&
    assert "1" "`c read 2`" "third item" &&
    ok || fail

log "exporting"
tmp=`mktemp`
trap "rm -f \"$tmp\"" INT TERM EXIT
c exporttab "$tmp" &&
    ok || fail

TAB=""
log "removing tab"
c renametab test2 test3
    ok || fail

log "importing tab"
c importtab "$tmp" &&
    ok || fail

log "checking imported tab content"
    assert "`c eval 'for (i = 0; i < 1000; ++i) {tab("test2");a=read(i);tab("test3");b=read(i);if(str(a)!=str(b))break;};i'`" 1000
        ok || fail

TAB=test2
log "checking items in the imported tab"
    assert "3" "`c read 0`" "first item" &&
    assert "2" "`c read 1`" "second item" &&
    assert "1" "`c read 2`" "third item" &&
    ok || fail

log "summary"
[ $ERR -eq 0 ] && ok "(0 errors)" || fail "($ERR failed)"

c exit

