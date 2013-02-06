#!/bin/bash
# path to copyq binary
COPYQ=${1:-copyq}
# command to read clipboard
READ=${2:-"xclip -o"}
# command to write to clipboard
WRITE=${3:-"xclip"}

# count failed tests
FAILED=0
# result of last perf() call
T=0
# result if last testpl() call
LASTPL=0
# log filename
LOG="$0.log"
# last copyq exit code
EXIT=0
# testing tab name
TAB=test

# redirect to log file
> "$LOG"

# colorize output if possible
# usage: color {color_code} [format] {message}
color () {
    c=""
    c_end=""
    if [ -t 1 -a -z "$NOCOLOR" ]; then
        case "$1" in
            r) c="\e[0;31m" ;;
            g) c="\e[0;32m" ;;
            b) c="\e[0;36m" ;;
            y) c="\e[0;33m" ;;
        esac
        if [ -n "$c" ]; then
            c_end="\e[0m"
        fi
    fi
    shift

    if [ $# -gt 1 ]; then
        fmt=$1
        shift
    else
        fmt="%s\n"
    fi
    fmt="$c$fmt$c_end"

    printf "$fmt" "$@"
}

log () {
    color y "%-50s  " "* $@"
    echo -e "\n\n* $@" >> "$LOG"
}

ok () {
    if [ $EXIT -eq 0 ]; then
        color g "OK $@"
        echo "** OK" >> "$LOG"
    else
        fail
    fi
}

fail () {
    FAILED=$((FAILED+1))
    color r "FAILED! $@"
    [ -z "$ASSERTMSG" ] || color w "  $ASSERTMSG"
    [ $EXIT -eq 0 ] || color r "  exit code $EXIT"
    echo "** FAILED: $ASSERTMSG $@ (exit code $EXIT)" >> "$LOG"
    EXIT=0
    ASSERTMSG=""
    return 1
}

run () {
    args=${TAB:+tab $TAB}
    echo "*** $COPYQ $args $@" >> "$LOG"
    OUT=`"$COPYQ" $args "$@" 2>> "$LOG"`
    EXIT=$?
    [ $EXIT -eq 0 ] || echo "** exit code $EXIT" >> "$LOG"
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

# usage: assert {command} {value} [description]
assert () {
    result=$(eval "$1")
    exit_code=$?
    if [ $exit_code -ne 0 ]; then
        ASSERTMSG="$3: \"$1\" exit code is $exit_code)"
        return 1
    elif [ "$result" != "$2" ]; then
        ASSERTMSG="$3: \"$1\" returned \"$result\" != \"$2\")"
        return 1
    fi
}

start_server () {
    "$COPYQ" 2>> "$LOG" &
    pidof -s copyq 2>&1 > /dev/null
}

terminate_server () {
    "$COPYQ" exit &> /dev/null
    while pidof -s copyq 2>&1 > /dev/null; do
        sleep 1
    done
    return 0
}

restart_server ()
{
    log "restarting copyq server"
    { terminate_server && start_server; } 2>&1 > /dev/null &&
        ok || fail
}

clipboard ()
{
    sleep 0.5
    $READ
}

set_clipboard ()
{
    printf "%s" "$1" | $WRITE
}

ulimit -c unlimited

restart_server || exit 1

for tab in test test2 test3; do
    log "remove tab \"$tab\" if exists"
        tabs=$(run tab) || { fail; continue; }
        if echo "$tabs" | grep -q '^'"$tab"'$'; then
            run removetab "$tab" && ok || fail
        else
            ok
        fi
done

content="A B C D!"
log "clipboard"
    set_clipboard "$content" &&
    assert "run clipboard" "$(clipboard)" &&
    ok || fail

log "set clipboard"
    run add "$content" && sleep 0.25 &&
    assert "run clipboard" "$content" &&
    assert "clipboard" "$content" &&
    ok || fail

log "set clipboard with MIME"
    run write text/html "$content" && sleep 0.25 &&
    assert "run clipboard text/html" "$content" &&
    ok || fail

log "add items"
run add 3 2 1 0 &&
    assert "run read 0" "0" "first item" &&
    assert "clipboard" "0" "first item in clipboard" &&
    assert "run read 1" "1" "second item" &&
    assert "run read 2" "2" "third item" &&
    assert "run read 3" "3" "fourth item" &&
    ok || fail

log "remove items"
run remove &&
    assert "run read 0" "1" "first item after first removal" &&
    assert "clipboard" "1" "clipboard after first removal" &&
run remove &&
    assert "run read 0" "2" "first item after second removal" &&
    assert "clipboard" "2" "clipboard after second removal" &&
    ok || fail

log "move second item to clipboard"
i1=`run read 1`
run "select" 1
assert "run read 0" "$i1" && assert "clipboard" "$i1" &&
    ok || fail

log "read past end of list"
SIZE=`run size` &&
    X=`run read $SIZE` &&
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
    str=$x${str:+", $str"} &&
    testpl "$T <= 0.1" || break
done } && lastpl &&
    assert "run separator ', ' read $(echo {0..29})" "$str" &&
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
[ "`run read "$mime" 0`" = "`echo {0..99999}`" ] &&
    ok || fail

mime="text/plain"
log "adding huge amount of text in 1.0 seconds"
echo {0..99999}|perf write "$mime" - && testpl "$T <= 1.0" &&
    ok || fail "($T seconds)"

log "reading huge amount of text"
[ "`run read "$mime" 0`" = "`echo {0..99999}`" ] &&
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

restart_server || exit 1

log "checking items in the new tab"
    assert "run read 0" "3" "first item" &&
    assert "run read 1" "2" "second item" &&
    assert "run read 2" "1" "third item" &&
    ok || fail

log "exporting"
tmp=`mktemp`
trap "rm -f \"$tmp\"" INT TERM EXIT
run exporttab "$tmp" &&
    ok || fail

TAB=""
log "clipboard content"
for x in 1 2 3; do set_clipboard "... $x ..."; done && sleep 0.25 &&
    assert "run read" "... 3 ..." && run remove &&
for x in 4 5 6; do set_clipboard "... $x ..."; done && sleep 0.25 &&
    assert "run read" "... 6 ..." && run remove &&
    ok || fail

log "rename tab"
run renametab test2 test3 &&
    ok || fail

log "import tab"
run importtab "$tmp" &&
    ok || fail

log "checking imported tab content"
    assert "run eval 'for (i = 0; i < 1000; ++i) {tab(\"test2\");a=read(i);tab(\"test3\");b=read(i);if(str(a)!=str(b))break;}; print(i)'" 1000 &&
        ok || fail

TAB=test2
log "checking items in the imported tab"
    assert "run read 0" "3" "first item" &&
    assert "run read 1" "2" "second item" &&
    assert "run read 2" "1" "third item" &&
    ok || fail

log "summary"
[ $FAILED -eq 0 ] && ok "(0 failed)" || fail "($FAILED failed)"

terminate_server || fail

