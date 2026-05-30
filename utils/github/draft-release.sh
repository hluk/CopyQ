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
#   6. Verifies all expected files are present
#   7. Generates checksums and cosign signature
#   8. Creates (or updates) the draft release
#   9. Uploads ALL assets in one shot
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

expected_assets=(
    "CopyQ-${version}-x86_64.AppImage"
    "CopyQ-${version}-macos-12-m1.dmg"
    "CopyQ-${version}-macos-13.dmg"
    "copyq-${version}-setup.exe"
    "copyq-${version}.zip"
)

# Resolve the workflow run ID for a given workflow file.
get_run_id() {
    local workflow="$1"
    local tag_sha="$2"
    gh run list --workflow "$workflow" --commit "$tag_sha" \
        --limit 1 --json databaseId --jq '.[0].databaseId // empty'
}

# Download all artifacts from a workflow run, flattening into $workdir.
# gh run download creates subdirectories named after each artifact;
# we move files up one level into $workdir.
download_run_artifacts() {
    local run_id="$1"
    local tmpdir="$workdir/.dl-$$"
    mkdir -p "$tmpdir"

    gh run download "$run_id" --dir "$tmpdir" 2>/dev/null || true

    # Flatten: move files from subdirectories into $workdir
    find "$tmpdir" -mindepth 2 -type f | while IFS= read -r f; do
        local base
        base="$(basename "$f")"
        if [[ ! -f "$workdir/$base" ]]; then
            mv "$f" "$workdir/$base"
            log "  Downloaded: $base"
        fi
    done

    rm -rf "$tmpdir"
}

wait_and_download() {
    local tag_sha
    tag_sha="$(git rev-list -n 1 "$tag")"
    [[ -n "$tag_sha" ]] || die "Could not resolve commit SHA for tag $tag"

    local workflows=("build-linux.yml" "build-macos.yml" "build-windows.yml")
    local run_ids=()

    for wf in "${workflows[@]}"; do
        log "Getting run for $wf (commit $tag_sha) ..."
        local run_id
        run_id="$(get_run_id "$wf" "$tag_sha")"
        [[ -n "$run_id" ]] || die "Could not find run for $wf at commit $tag_sha"
        run_ids+=("$run_id")
    done

    for i in "${!workflows[@]}"; do
        log "Watching ${workflows[$i]} (run ${run_ids[$i]}) ..."
        gh run watch "${run_ids[$i]}" --exit-status --interval 30 \
            || die "${workflows[$i]} failed"
        log "Downloading artifacts from ${workflows[$i]} ..."
        download_run_artifacts "${run_ids[$i]}"
    done

    log "All builds completed and artifacts downloaded"
}

# Check if all expected assets exist locally.
has_all_local() {
    for name in "${expected_assets[@]}"; do
        [[ -f "$workdir/$name" ]] || return 1
    done
    return 0
}

if has_all_local; then
    log "All expected build artifacts already in $workdir"
else
    wait_and_download
    if ! has_all_local; then
        log "Files in $workdir:"
        ls -1 "$workdir" >&2
        die "CI artifacts downloaded but not all expected files are present"
    fi
fi

# ---------------------------------------------------------------------------
# Checksums and cosign signature (skip if cosign.bundle exists)
# ---------------------------------------------------------------------------

all_assets=(
    "CopyQ-${version}.tar.gz"
    "${expected_assets[@]}"
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
for name in "${expected_assets[@]}"; do
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
