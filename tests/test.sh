#!/bin/bash
# Testing suite for CopyQ application.
copyq=${1:-copyq}

# log file for copyq server
LOG="$0.log"
>"$LOG"

TEMP="$0.out"

if [ -t 1 -a "$NOCOLOR" != "1" ]; then
    export HAS_COLOR=1
fi

# colorize output if possible
# usage: color {color_code} [format] {message}
color () {
    c=""
    c_end=""
    if [ "$HAS_COLOR" = "1" ] ; then
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
        fmt="%s"
    fi
    fmt="$c$fmt$c_end"

    printf "$fmt" "$@" 1>&2
}

assert() {
    local expected_exit=$1
    local expected_output=$2
    shift 2

    # Keyword "local" eats exit status!
    actual_output=$("$@")
    actual_exit=$?

    local msg_exit=""
    local msg_output=""

    if [ "$actual_exit" != "$expected_exit" ]; then
        msg_exit="Exit status of \"$@\" is $actual_exit but should be $expected_exit!"
    fi

    if [ "$IGNORE_OUTPUT" != "1" -a "$actual_output" != "$expected_output" ]; then
        msg_output="Unexpected output of \"$@\"!"
        # TODO: Also print expected and actual outputs.
    fi

    if [ -n "$msg_exit" -o -n "$msg_output" ]; then
        lineno=$(caller 1 | cut -d ' ' -f 1)
        color "r" "  Line $lineno: "
        [ -z "$msg_exit" ] || color "w" "%s\n" "$msg_exit"
        [ -z "$msg_output" ] || color "w" "%s\n    expected: %s\n    actual: %s\n" \
            "$msg_output" "$expected_output" "$actual_output"
        exit 1
    fi
}

assert_true() {
    IGNORE_OUTPUT=1 assert 0 "" "$@"
}

assert_false() {
    IGNORE_OUTPUT=1 assert 1 "" "$@"
}

assert_equals() {
    assert 0 "$@"
}

# print label
print_label() {
    color "y" "%32s" "$@ ... "
}

# print OK or FAILED
print_status() {
    local t=$1
    [ -n "$t" ] && color "g" "OK (in $t s)" || color "r" "FAILED!"
    echo
}

# print current test number and number of tests
print_counter() {
    color "b" "[$1/$2] "
}

# run copyq
run() {
    "$copyq" "$@"
}

is_server_running() {
    run size &>/dev/null
}

start_server() {
    nohup "$copyq" &>> "$LOG" &

    tries=20
    while [ $((--tries)) -ge 0 ] && ! is_server_running; do
        sleep 0.1
    done

    assert_true is_server_running
}

stop_server() {
    assert_true run exit >/dev/null
    assert_false is_server_running
}

restart_server() {
    is_server_running && stop_server
    start_server
}

has_tab() {
    { run tab || assert false; } | grep -q '^'"$1"'$'
}

# Called before all tests are executed.
init_tests() {
    restart_server
}

# Called after all tests are executed.
finish_tests() {
    has_tab "test" &&
        assert_true run removetab "test"
    assert_false has_tab "test"
    assert_true stop_server
}

# Called befor each test is executed.
init_test() {
    assert_true is_server_running
    has_tab "test" &&
        assert_true run removetab "test"
    assert_false has_tab "test"
}

test_add_remove_test_tab() {
    assert_true run tab "test" add 0
    assert_true has_tab "test"
    assert_true run removetab "test"
    assert_false has_tab "test"
}

test_restore_tab() {
    assert_true run tab "test" add a b c d
    assert_true has_tab "test"

    restart_server

    assert_true has_tab "test"
    assert_equals "4" run tab "test" size
    assert_equals "d" run tab "test" read 0
    assert_equals "c" run tab "test" read 1
    assert_equals "b" run tab "test" read 2
    assert_equals "a" run tab "test" read 3

    assert_true run removetab "test"

    restart_server

    assert_false has_tab "test"
}

run_test() {
    local test_fn=$1
    if [ "$test_fn" = "init_tests" -o "$test_fn" = "finish_tests" ]; then
        "$test_fn"
    else
        init_test && "$test_fn"
    fi 2>&1
}

run_tests() {
    local tests=($(compgen -A function | grep '^test_'))
    count=${#tests[*]}

    i=0
    failed=0
    for test_fn in init_tests "${tests[@]}" finish_tests; do
        if [ $i -eq 0 -o $i -gt $count ]; then
            print_counter "*" "*"
        else
            print_counter $i $count
        fi

        print_label "$test_fn"

        t=$({ time -p run_test "$test_fn" > "$TEMP" || exit 1; } 2>&1 | awk '{print $2;exit}')

        if [ -z "$t" ]; then
            failed=$((failed+1))
        fi

        print_status "$t"

        output=$(cat "$TEMP")
        if [ -n "$output" ]; then
            echo "$output"
        fi

        i=$((i + 1))
    done

    if [ $failed = 0 ]; then
        color "g" "All OK."
    else
        color "r" "Failed tests: $failed"
    fi
    echo
}

run_tests

