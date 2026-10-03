#!/usr/bin/env bash
#
# Creates a draft GitHub release for the latest tag with all artifacts.
#
# This is the single entry point for release creation.  CI workflows
# (build-linux.yml, build-macos.yml, build-windows.yml) only build and
# store workflow artifacts via actions/upload-artifact.  This script:
#
#   1. Validates version / tag / CHANGES.md
#   2. Extracts the changelog body
#   3. Creates a source tarball
#   4. Waits for CI workflow runs to complete
#   5. Downloads workflow artifacts locally
#   6. Generates checksums and cosign signature
#   7. Creates (or updates) the draft release
#   8. Uploads ALL assets in one shot
#
# Idempotency: reuse the same WORKDIR to resume an interrupted run.
# Each step skips work that is already done.
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

set -euo pipefail


die() { printf 'Error: %s\n' "$1" >&2; exit 1; }
log() { printf '==> %s\n' "$1" >&2; }

# ---------------------------------------------------------------------------
# Validate version and tag
# ---------------------------------------------------------------------------

repo_root="$(git rev-parse --show-toplevel)"
changes_file="$repo_root/CHANGES.md"
[[ -f "$changes_file" ]] || die "CHANGES.md not found at $changes_file"

version="${1:-}"
[[ -n "$version" ]] || die "Usage: $0 VERSION [WORKDIR]"

tag="$(git describe --tags --abbrev=0 HEAD)"
[[ -n "$tag" ]] || die "No tags found"

expected_tag="v$version"
[[ "$tag" = "$expected_tag" ]] || die "Latest tag is $tag but expected $expected_tag"

# CHANGES.md must start with this version's section header.
changes_header="$(head -n1 "$changes_file")"
expected_header="# $version"
[[ "$changes_header" = "$expected_header" ]] || die "CHANGES.md starts with '$changes_header' but expected '$expected_header'"
log "Version: $version  (tag: $tag)"

workdir="${2:-release-$version}"
mkdir -p "$workdir"
workdir="$(cd "$workdir" && pwd)"
log "Working directory: $workdir"

# ---------------------------------------------------------------------------
# Extract changelog
# ---------------------------------------------------------------------------

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
[[ -n "$changelog_body" ]] || die "No changelog entry for version $version in CHANGES.md"
log "Extracted changelog ($(echo "$changelog_body" | wc -l) lines)"

# ---------------------------------------------------------------------------
# Create source tarball (skip if exists)
# ---------------------------------------------------------------------------

source_tarball="$workdir/CopyQ-$version.tar.gz"
if [[ -f "$source_tarball" ]]; then
    log "Source tarball already exists: $source_tarball"
else
    log "Creating source tarball ..."
    git -C "$repo_root" archive --format=tar.gz \
        --prefix="CopyQ-$version/" --output="$source_tarball" "$tag"
fi

# ---------------------------------------------------------------------------
# Wait for CI builds and download artifacts
# ---------------------------------------------------------------------------

# Resolve the workflow run ID for a given workflow file.
get_run_id() {
    local workflow="$1"
    local tag_sha="$2"
    gh run list --workflow "$workflow" --branch "$tag" \
        --limit 1 --json databaseId --jq '.[0].databaseId // empty'
}

linux_run_id="$(get_run_id "build-linux.yml" "$tag")"
macos_run_id="$(get_run_id "build-macos.yml" "$tag")"
windows_run_id="$(get_run_id "build-windows.yml" "$tag")"

log "Watching workflows..."
gh run watch "$linux_run_id" --exit-status --interval 30
gh run watch "$macos_run_id" --exit-status --interval 30
gh run watch "$windows_run_id" --exit-status --interval 30

expected_assets=(
    "CopyQ-${version}-x86_64.AppImage:$linux_run_id"
    "CopyQ-${version}-macos-13.dmg:$macos_run_id"
    "CopyQ-${version}-macos-13-m1.dmg:$macos_run_id"
    "copyq-${version}-setup.exe:$windows_run_id"
    "copyq-${version}.zip:$windows_run_id"
)

for asset_mapping in "${expected_assets[@]}"; do
    asset_name=$(cut -f1 -d: <<<"$asset_mapping")
    run_id=$(cut -f2 -d: <<<"$asset_mapping")
    if [[ ! -f "$workdir/$asset_name" ]]; then
        log "Fetching $asset_name from run ID $run_id..."
        asset_id=$(gh api repos/:owner/:repo/actions/runs/$run_id/artifacts \
            --jq '.artifacts[]|select(.name=="'"$asset_name"'")|.id')

        target=$workdir/$asset_name
        gh api /repos/:owner/:repo/actions/artifacts/"$asset_id"/zip > "$target"

        if [[ $(stat -c%s "$target") -lt 100000 ]]; then
            die "Failed to fetch artifact"
        fi
    fi
done

log "All builds completed and artifacts downloaded"

# ---------------------------------------------------------------------------
# Checksums and cosign signature (skip if cosign.bundle exists)
# ---------------------------------------------------------------------------

all_assets=(
    "CopyQ-${version}.tar.gz"
    "${expected_assets[@]/:*}"
)

checksums_file="$workdir/checksums-sha512.txt"
cosign_bundle="$workdir/cosign.bundle"

if [[ -f "$cosign_bundle" ]]; then
    log "Checksums and signature already exist"
else
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

# ---------------------------------------------------------------------------
# Create or update the draft release
# ---------------------------------------------------------------------------

if gh release view "$tag" --json tagName >/dev/null 2>&1; then
    log "Release $tag exists — updating title and body"
    gh release edit "$tag" --draft --title "$version" --notes "$changelog_body"
else
    log "Creating draft release for $tag"
    gh release create "$tag" --draft --title "$version" --notes "$changelog_body"
fi

# ---------------------------------------------------------------------------
# Upload all assets
# ---------------------------------------------------------------------------

upload_files=("$source_tarball")
for name in "${expected_assets[@]/:*}"; do
    upload_files+=("$workdir/$name")
done
upload_files+=("$checksums_file" "$cosign_bundle")

log "Uploading ${#upload_files[@]} assets to release $tag ..."
gh release upload "$tag" "${upload_files[@]}" --clobber

log "Done: $(gh release view "$tag" --json url --jq '.url')"
log ""
log "Remaining manual steps:"
log "  - Review the draft release on GitHub and publish it"
log "  - Upload packages to SourceForge"
log "  - Update Flathub package"
log "  - Write release announcement to CopyQ group"
