This is step-by-step description on how to release new version of CopyQ.

# Verify the Builds

- [copyq-beta Ubuntu package](https://launchpad.net/~hluk/+archive/ubuntu/copyq-beta)
- [copyq on OBS](https://build.opensuse.org/package/show/home:lukho:copyq/CopyQ-Qt5)
- [Windows builds](https://ci.appveyor.com/project/hluk/copyq)

# Update Version and Changelog

Update `CHANGES.md` file (go through all commits since the last release tag).

Bump the version:

    utils/bump_version.sh 9.1.0

Verify and push the changes:

    for r in origin gitlab; do git push --follow-tags $r master || break; done

# Launchpad: Build Ubuntu Packages

Upload source files for [copyq Ubuntu package](https://launchpad.net/~hluk/+archive/ubuntu/copyq):

    git checkout v9.1.0
    utils/debian/create_source_packages.sh
    dput ppa:hluk/copyq ../copyq_9.1.0~*.changes

# openSUSE Build Service: Build Other Linux Packages

Build on [OBS](https://build.opensuse.org/package/show/home:lukho:copyq/CopyQ-Qt5):

    osc co home:lukho:copyq
    cd home:lukho:copyq/CopyQ-Qt5
    bash create_beta_package.sh
    $EDITOR debian.changelog
    osc commit

NOTE: In case of system package conflicts like the following one, update
[project
configuration](https://build.opensuse.org/projects/home:lukho:copyq/prjconf)
(for example: `Prefer: clang13-libs util-linux-core`).

    have choice for libclang.so.13()(64bit) needed by qt5-doctools: clang-libs
    clang13-libs, have choice for libclang.so.13(LLVM_13)(64bit) needed by
    qt5-doctools: clang-libs clang13-libs, have choice for (util-linux-core or
    util-linux) needed by systemd: util-linux util-linux-core

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

      $COPYQ_SOURCE/utils/download_window_builds.sh 9.1.0

- Binaries for macOS and AppImage from [github](https://github.com/hluk/CopyQ/releases)
- Create source package:

      $COPYQ_SOURCE/utils/create_source_package.sh 9.1.0

- OBS packages:

      $COPYQ_SOURCE/utils/download_obs_packages.sh 9.1.0 1.1

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
- [fosshub](https://www.fosshub.com/CopyQ.html)

      ./utils/fosshub.py 9.1.0 $TOKEN

Update Homebrew package for macOS:

    brew install vitorgalvao/tiny-scripts/cask-repair
    brew upgrade cask-repair
    cask-repair copyq

Write release announcement to [CopyQ group](https://groups.google.com/forum/#!forum/copyq).
