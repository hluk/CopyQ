#!/bin/bash
#
# Updates CopyQ version in source code.
#
# Argument is the new version.
#
# 1. Checks new version format.
# 2. Checks if CHANGES file contains the new version.
# 3. Updates version in version header.
# 4. Updates version for plugins.
# 5. Updates version in AppData file.
#

set -euo pipefail

version=$1

version_file=version.cmake
appdata_file=shared/com.github.hluk.copyq.appdata.xml
itemwidget_file=src/item/itemwidget.h
changes_file=CHANGES

check_version_format() {
    if ! grep -q '^[0-9]\+\.[0-9]\+\.[0-9]\+$' <<< "$version"; then
        echo "Expected version format is MAJOR.MINOR.PATCH"
        exit 1
    fi
}

check_changes() {
    last_changes_version=$(head -1 "$changes_file")
    if [[ "$last_changes_version" != "v$version" ]]; then
        echo "Update $changes_file first"
        exit 1
    fi
}

fix_file() {
    file=$1
    format=$2

    pattern=$(printf "$format" '[0-9]\+\.[0-9]\+\.[0-9]\+')
    text=$(printf "$format" "$version")
    sed -i "s|$pattern|$text|" "$file"

    new_version=$(grep -o "$pattern" "$file")
    if [[ "$new_version" != "$text" ]]; then
        echo "Failed to replace version in $file"
        exit 1
    fi
}

fix_version_file() {
    fix_file "$version_file" 'set(copyq_version "v%s")'
}

fix_itemwidget() {
    fix_file "$itemwidget_file" \
        '#define COPYQ_PLUGIN_ITEM_LOADER_ID "com.github.hluk.copyq.itemloader/%s"'
}

fix_appdata() {
    sed -i '/<release version="'"$version"'"/d' "$appdata_file"

    if grep -qo '"'"$version"'"' "$appdata_file"; then
        echo "New version already mentioned in $appdata_file"
        exit 1
    fi

    release_date=$(date +%Y-%m-%d)
    release_node="<release version=\"$version\" date=\"$release_date\" />"
    sed -i 's#^\(\s*\)<releases>$#&\n\1    '"$release_node"'#' "$appdata_file"

    if ! grep -qo '<release version="'"$version"'"' "$appdata_file"; then
        echo "Failed to add new version to $appdata_file"
        exit 1
    fi

    appstream-util validate-relax --nonet "$appdata_file"
}

check_version_format
check_changes
fix_version_file
fix_itemwidget
fix_appdata
git commit -a -m "v$version"
git tag -s -a -m "v$version" "v$version"
git show
