#!/usr/bin/env bash
#
# Drafts a GitHub release for the latest tag.
#
#
# CI workflows (build-macos.yml, build-windows.yml) attach .dmg, .zip,
# and .exe files to the draft release directly. This script adds: source
# tarball, checksums-sha512.txt, and cosign.bundle.
#
# Requirements:
#   - gh CLI (authenticated)
#   - git (with tags fetched)
#   - cosign (for signing)
#   - Run from anywhere inside the repository
#
# Usage:
#   utils/github/draft-release.sh VERSION [WORKDIR]
#
#   VERSION  Expected version, e.g. 14.0.0. Must match the latest git tag.
#   WORKDIR  Directory for artifacts (default: ./release-VERSION).
#            Reuse the same directory to resume an interrupted run.

set -euo pipefail

readonly POLL_INTERVAL=60
readonly MAX_WAIT=7200

die() { printf 'Error: %s\n' "$1" >&2; exit 1; }
log() { printf '==> %s\n' "$1" >&2; }

# resolve repo root and validate version against latest tag

repo_root="$(git rev-parse --show-toplevel)"
changes_file="$repo_root/CHANGES.md"
[ -f "$changes_file" ] || die "CHANGES.md not found at $changes_file"

version="${1:-}"
[ -n "$version" ] || die "Usage: $0 VERSION [WORKDIR]"

tag="$(git describe --tags --abbrev=0 HEAD)"
[ -n "$tag" ] || die "No tags found"

expected_tag="v$version"
[ "$tag" = "$expected_tag" ] || die "Latest tag is $tag but expected $expected_tag"

# CHANGES.md must start with this version's section header.
changes_header="$(head -n1 "$changes_file")"
expected_header="# $version"
[ "$changes_header" = "$expected_header" ] || die "CHANGES.md starts with '$changes_header' but expected '$expected_header'"
log "Version: $version  (tag: $tag)"

workdir="${2:-release-$version}"
mkdir -p "$workdir"
workdir="$(cd "$workdir" && pwd)"
log "Working directory: $workdir"

# extract changelog and create/update draft release

extract_changelog() {
    awk -v ver="$version" '
        BEGIN { found = 0 }
        /^# / {
            if (found) exit
            gsub(/^# v?/, "", $0)
            if ($0 == ver) { found = 1; next }
        }
        found { print }
    ' "$changes_file" | sed -e '/./,$!d' -e :a -e '/^\n*$/{ $d; N; ba; }'
}

changelog_body="$(extract_changelog)"
[ -n "$changelog_body" ] || die "No changelog entry for version $version in CHANGES.md"
log "Extracted changelog ($(echo "$changelog_body" | wc -l) lines)"

if gh release view "$tag" --json tagName >/dev/null 2>&1; then
    log "Release $tag exists — updating title and body"
    gh release edit "$tag" --draft --title "$version" --notes "$changelog_body"
else
    log "Creating draft release for $tag"
    gh release create "$tag" --draft --title "$version" --notes "$changelog_body"
fi

# create source tarball and upload

source_tarball="$workdir/CopyQ-$version.tar.gz"
if [ -f "$source_tarball" ]; then
    log "Source tarball already exists: $source_tarball"
else
    log "Creating source tarball ..."
    git -C "$repo_root" archive --format=tar.gz \
        --prefix="Copyq-$version/" --output="$source_tarball" "$tag"
fi

log "Uploading source tarball to release $tag ..."
gh release upload "$tag" "$source_tarball" --clobber

# wait for CI build artifacts on the release

expected_assets=(
    "CopyQ-${version}-macos-12-m1.dmg"
    "CopyQ-${version}-macos-13.dmg"
    "copyq-${version}-setup.exe"
    "copyq-${version}.zip"
)

has_all_assets() {
    local assets
    assets="$(gh release view "$tag" --json assets --jq '.assets[].name')"
    for name in "${expected_assets[@]}"; do
        if ! echo "$assets" | grep -qxF "$name"; then
            return 1
        fi
    done
    return 0
}

if has_all_assets; then
    log "All expected build artifacts already on the release"
else
    log "Waiting for CI to attach build artifacts ..."
    elapsed=0
    while ! has_all_assets; do
        if [ "$elapsed" -ge "$MAX_WAIT" ]; then
            log "Current assets:"
            gh release view "$tag" --json assets --jq '.assets[].name' >&2
            die "Timed out waiting for build artifacts on release $tag"
        fi
        log "  Not all artifacts present yet, checking again in ${POLL_INTERVAL}s ..."
        sleep "$POLL_INTERVAL"
        elapsed=$((elapsed + POLL_INTERVAL))
    done
    log "All expected build artifacts are now on the release"
fi

# download assets, checksum, and sign

all_assets=(
    "CopyQ-${version}.tar.gz"
    "${expected_assets[@]}"
)

checksums_file="$workdir/checksums-sha512.txt"
cosign_bundle="$workdir/cosign.bundle"

if [ -f "$cosign_bundle" ]; then
    log "Checksums and signature already exist"
else
    log "Downloading release assets ..."
    for name in "${expected_assets[@]}"; do
        gh release download "$tag" --dir "$workdir" --clobber \
            --pattern "$name"
    done

    log "Generating checksums ..."
    (
        cd "$workdir"
        sha512sum "${all_assets[@]}" > checksums-sha512.txt
    )
    log "Checksums:"
    cat "$checksums_file" >&2

    log "Signing with cosign (browser will open for OIDC) ..."
    cosign sign-blob "$checksums_file" --bundle "$cosign_bundle"

    cosign verify-blob "$checksums_file" --bundle "$cosign_bundle" \
        --certificate-identity=hluk@email.cz \
        --certificate-oidc-issuer=https://github.com/login/oauth
    log "Signature verified"
fi

# upload checksums and signature

log "Uploading checksums and signature to release $tag ..."
gh release upload "$tag" \
    "$checksums_file" \
    "$cosign_bundle" \
    --clobber

log "Done: $(gh release view "$tag" --json url --jq '.url')"
log ""
log "Remaining manual steps:"
log "  - Review the draft release on GitHub and publish it"
log "  - Upload packages to SourceForge"
log "  - Update Flathub package"
log "  - Write release announcement to CopyQ group"