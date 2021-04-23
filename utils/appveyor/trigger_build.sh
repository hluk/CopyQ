#!/bin/bash
set -exuo pipefail
commit=$1
branch=${2:-master}
url=https://ci.appveyor.com/api/builds
args=(
    # https://ci.appveyor.com/api-keys
    -H "Authorization: Bearer ${APPVEYOR_TOKEN}"
    -H "Content-Type: application/json"
    --data-binary
    '{"accountName": "hluk", "projectSlug": "copyq", "branch": "'"$branch"'", "commitId": "'"$commit"'"}'
    "$url"
)
exec curl "${args[@]}"
