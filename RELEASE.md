This is step-by-step description on how to release new version of CopyQ.

# Verify the Builds

- [copyq-beta Ubuntu package](https://launchpad.net/~hluk/+archive/ubuntu/copyq-beta)
- [copyq on OBS](https://build.opensuse.org/package/show/home:lukho:copyq/CopyQ-Qt5)
- [Windows builds](https://ci.appveyor.com/project/hluk/copyq)

# Update Version and Changelog

Update `CHANGES.md` file.

Bump version.

    utils/bump_version.sh 7.1.0

Verify and push the changes.

    for r in origin gitlab bitbucket; do git push --follow-tags $r master || break; done

# Build Packages

Upload source files for [copyq Ubuntu package](https://launchpad.net/~hluk/+archive/ubuntu/copyq).

    git checkout v7.1.0
    utils/debian/create_source_packages.sh
    dput ppa:hluk/copyq ../copyq_7.1.0~*.changes

Build on [OBS](https://build.opensuse.org/package/show/home:lukho:copyq/CopyQ-Qt5).

    osc co home:lukho:copyq
    cd home:lukho:copyq/CopyQ-Qt5
    ./create_beta_package.sh
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

Update [flathub package](https://github.com/flathub/com.github.hluk.copyq):

1. Update "tag" and "commit" in "com.github.hluk.copyq.json" file.
2. Push to your fork.
3. [Create pull request](https://github.com/flathub/com.github.hluk.copyq/compare/master...hluk:master).
4. Wait for build to finish (flathubbot will add comments).
5. [Verify the build](https://flathub.org/builds/#/).
6. Merge the changes if build is OK.

# Publish Release

Download:

- binaries for Windows from [AppVeyor](https://ci.appveyor.com/project/hluk/copyq)

      utils/download_window_builds.sh 7.1.0

- binary for OS X from [github](https://github.com/hluk/CopyQ/releases)
- source package from [github](https://github.com/hluk/CopyQ/releases)
- OBS packages

      utils/download_obs_packages.sh 7.1.0 1.1

Create [release on github](https://github.com/hluk/CopyQ/releases) for the new version tag.

Upload packages and binaries to:

- [github](https://github.com/hluk/CopyQ/releases)
- [sourceforge](https://sourceforge.net/projects/copyq/files/)
- [fosshub](https://www.fosshub.com/CopyQ.html)

        ./utils/fosshub.py 7.1.0 $TOKEN

Update Homebrew package for OS X.

    brew install vitorgalvao/tiny-scripts/cask-repair
    brew upgrade cask-repair
    cask-repair copyq

Write release announcement to [CopyQ group](https://groups.google.com/forum/#!forum/copyq).
