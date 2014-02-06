#!/bin/bash
# Rebase last commit from target branch to source branch and push.
source_branch="master"
target_branch="travis-mac"
remote="origin"
msg="Updated Travis CI build file for OS X"
keep_files=(
    .travis.yml
)

branch_name () {
    git rev-parse --abbrev-ref "$1"
}

current_branch=$(branch_name HEAD)

if [ "$current_branch" == "$source_branch" ]; then
    echo "** Rebasing last commit from \"$target_branch\" to \"$source_branch\"."
    git checkout -b "$target_branch" &&
        git checkout "$remote/$target_branch" -- "${keep_files[@]}" &&
        git add "${keep_files[@]}" &&
        git commit --message "$msg" &&
        git push --force "$remote" "HEAD:$target_branch" ||
        echo "** Failed to update \"$target_branch\" branch!"
    git checkout "$current_branch"
    git branch -D "$target_branch"
fi

