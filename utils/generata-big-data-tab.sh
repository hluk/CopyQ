#!/bin/bash
set -euo pipefail

COPYQ=${COPYQ:-copyq}
session=test
tab=BIG

items=10000
size=50000

for x in $(seq $items); do
    echo $x
    head -c $size /dev/random | base64 | "$COPYQ" -s $session tab $tab insert -1 -
done
