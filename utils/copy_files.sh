#!/bin/bash
# Save whole files in CopyQ.
# USAGE:
#   utils/copy_files.sh [FILES...]
# EXAMPLE:
#   TAB=images utils/copy_files.sh ~/Pictures/*.jpg
COPYQ=${COPYQ:-copyq}
TAB=${TAB:-files}

set -e

i=0
for x in "$@"; do
    # get MIME
    mime=$(file -b -L --mime-type "$x")
    # print info
    printf "%s: %s\n" "$mime" "$x"
    # write image with label
    "$COPYQ" -s "$SESSION" tab "$TAB" write $((++i)) \
        "application/x-copyq-item-notes" "$x" \
        "$mime" - < "$x"
done

