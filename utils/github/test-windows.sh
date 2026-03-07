#!/usr/bin/bash
# Runs CopyQ tests on Windows CI (GitHub Actions).
#
# Expected environment variables:
#   COPYQ_PLUGINS  - path to test plugin DLL
#   GPGPATH        - path to GPG binaries
#
# Run from the build directory containing copyq-tests.exe.
set -xuo pipefail

export PATH="$GPGPATH:$PATH"
mkdir -p ~/.gnupg
chmod go-rwx ~/.gnupg
gpg --version

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)

# shellcheck source=../windows-test-skip-list.sh
source "$SCRIPT_DIR/../windows-test-skip-list.sh"

TESTS_BIN=./copyq-tests.exe

# Build an associative array for O(1) lookup.
declare -A skip_set
for t in "${SKIP_TESTS[@]}"; do
    skip_set[$t]=1
done

# Get all core test names from the binary, subtract the skip list,
# and pass the remaining tests as positional arguments.
all_tests=$("$TESTS_BIN" -functions 2>&1 | sed -n 's/()$//p')
run_tests=()
for t in $all_tests; do
    if [[ -z ${skip_set[$t]:-} ]]; then
        run_tests+=("$t")
    fi
done

echo "Running ${#run_tests[@]} of $(echo "$all_tests" | wc -w) core tests (skipping ${#SKIP_TESTS[@]} platform-independent tests)"

exit_code=0

# Run selected core tests first. Passing test names as arguments disables
# plugin tests (see tests.cpp), so we run plugins separately below.
"$TESTS_BIN" "${run_tests[@]}" || exit_code=$?

echo "Core tests exit code: $exit_code"

# Run all plugin tests (these exercise Windows-specific plugin behavior).
"$TESTS_BIN" PLUGINS:. || {
    plugin_exit=$?
    echo "Plugin tests exit code: $plugin_exit"
    exit_code=$((exit_code > 0 ? exit_code : plugin_exit))
}

exit $exit_code
