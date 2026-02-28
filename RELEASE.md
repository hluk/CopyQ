This is step-by-step description on how to release new version of CopyQ.

# Update Version and Changelog

Update `CHANGES.md` file (go through all commits since the last release tag).

Bump the version:

    utils/bump_version.sh 14.0.0

Verify and push the changes:

    for r in origin gitlab; do git push --follow-tags $r master || break; done

# Build Flatpak

Update [flathub package](https://github.com/flathub/com.github.hluk.copyq):

1. Update "tag" and "commit" in "com.github.hluk.copyq.json" file.
2. Push to your fork.
3. [Create pull request](https://github.com/flathub/com.github.hluk.copyq/compare/master...hluk:master).
4. Verify the build when the build finishes (flathubbot will add comments).
5. Merge the changes if the build is OK.

# Download Packages

Download:

- Binaries for Windows from [AppVeyor](https://ci.appveyor.com/project/hluk/copyq):

      $COPYQ_SOURCE/utils/download_window_builds.sh 14.0.0

- Binaries for OS X from [github](https://github.com/hluk/CopyQ/releases)
- Create source package:

      $COPYQ_SOURCE/utils/create_source_package.sh 14.0.0

# Checksums and Signing

Create checksums and sign all new packages, source tarball and binaries:

    $COPYQ_SOURCE/utils/sign_released_files.sh

This creates `checksums-sha512.txt` with the checksums and its signature in
`cosign.bundle`.

# Publish Release

Create [release on GitHub](https://github.com/hluk/CopyQ/releases) for the new version tag.

Upload packages and binaries to:

- [github](https://github.com/hluk/CopyQ/releases) (include `checksums-sha512.txt` and `cosign.bundle`)
- [sourceforge](https://sourceforge.net/projects/copyq/files/)

Write release announcement to [CopyQ group](https://groups.google.com/forum/#!forum/copyq).
