#!/bin/bash
set -ex

dir=$(dirname "$(readlink -f "$0")")
spec_template=$dir/copyq.spec
repo=$USER/copyq

revision=$(git rev-parse HEAD)
rev=$(git rev-parse --short HEAD)
branch=$(git rev-parse --abbrev-ref HEAD)
last_version=$(git describe --tags --abbrev=0)
date=$(date -u '+%Y%m%d')
sed_script=$(cat <<EOF
s/_BRANCH_/$branch/;
s/_LAST_VERSION_/$last_version/;
s/_REVISION_/$revision/;
s/_REV_/$rev/;
s/_DATE_/$date/;
EOF
)

spec=$(mktemp copyq-XXXXXXXXXX.spec)
clean() {
    rm -f -- "$spec"
}
trap clean QUIT TERM INT HUP EXIT

sed "$sed_script" "$spec_template" > "$spec"
${EDITOR:-vi} "$spec"

rpmlint "$spec"
spectool -g --sourcedir "$spec"
rpmbuild -bs "$spec"

cat <<EOF
Upload the SRPM to build on Copr:

    copr-cli build $repo <SRPM>

Install the package using:

    sudo dnf install 'dnf-command(copr)'
    sudo dnf copr enable $repo
    sudo dnf install copyq
EOF
