#!/bin/bash
set -e

email=$1
context=$(dirname "$0")

cd "$context"

trap 'rm -f launchpad.key' QUIT TERM INT EXIT
gpg2 --export-secret-keys "$email" > launchpad.key

exec podman build -t launchpad .
