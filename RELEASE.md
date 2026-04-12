This is step-by-step description on how to release new version of CopyQ.

# Update Version and Changelog

Update `CHANGES.md` file (go through all commits since the last release tag).

Bump the version:

    utils/bump_version.sh 14.0.0

Verify and push the changes:

    for r in origin gitlab; do git push --follow-tags $r master || break; done

# Draft Release

Run the release script:

    utils/github/draft-release.sh 14.0.0

This automates the following steps:

1. Creates a draft GitHub Release with the changelog from `CHANGES.md`.
2. Creates the source tarball and uploads it to the release.
3. Waits for CI to attach build artifacts (`.dmg`, `.zip`, `.exe`) to the release.
4. Downloads the release assets, generates `checksums-sha512.txt`, and signs it
   with `cosign` (opens a browser for OIDC authentication).
5. Uploads `checksums-sha512.txt` and `cosign.bundle` to the release.

The script is idempotent. If interrupted, rerun it with the same working
directory to resume:

    utils/github/draft-release.sh 14.0.0 ./release-14.0.0

Artifacts produced by CI (attached automatically by GitHub Actions):

- Windows installer (`copyq-VERSION-setup.exe`)
- Windows portable zip (`copyq-VERSION.zip`)
- macOS Intel DMG (`CopyQ-VERSION-macos-13.dmg`)
- macOS Apple Silicon DMG (`CopyQ-VERSION-macos-12-m1.dmg`)
- Linux AppImage (`CopyQ-VERSION-x86_64.AppImage`)

# Build Flatpak

Update [flathub package](https://github.com/flathub/com.github.hluk.copyq):

1. Update "tag" and "commit" in "com.github.hluk.copyq.json" file.
2. Push to your fork.
3. [Create pull request](https://github.com/flathub/com.github.hluk.copyq/compare/master...hluk:master).
4. Verify the build when the build finishes (flathubbot will add comments).
5. Merge the changes if the build is OK.

# Publish Release

Review and publish the draft release on GitHub.

Upload packages and binaries to:

- [sourceforge](https://sourceforge.net/projects/copyq/files/)

Write release announcement to [CopyQ group](https://groups.google.com/forum/#!forum/copyq).
