This is step-by-step description on how to release new version of CopyQ.

Test the builds (optional step):
- [copyq-beta Ubuntu package](https://launchpad.net/~hluk/+archive/ubuntu/copyq-beta)
- [copyq-beta on OBS](https://build.opensuse.org/package/show/home:lukho:copyq-beta/CopyQ-Qt5-beta)

Update CHANGES file.

Bump version.

    utils/bump_version.sh 3.3.1

Commit changes and create version tag.

    git commit -a -m v3.3.1
    git tag -s -a -m v3.3.1 v3.3.1

Push the changes.

Create [release on github](https://github.com/hluk/CopyQ/releases) for the new version tag.

Upload source files for [copyq Ubuntu package](https://launchpad.net/~hluk/+archive/ubuntu/copyq).

    utils/debian/create_source_packages.sh
    cd ..
    dput ppa:hluk/copyq copyq_3.3.1~*.changes

Build on [OBS](https://build.opensuse.org/package/show/home:lukho:copyq/CopyQ-Qt5).

    osc co home:lukho:copyq
    cd home:lukho:copyq/CopyQ-Qt5
    ./create_beta_package.sh
    $EDITOR debian.changelog
    osc commit

Download:
- binaries for Windows from [AppVeyor](https://ci.appveyor.com/project/hluk/copyq).
- binary for OS X from [github](https://github.com/hluk/CopyQ/releases).
- source package from [github](https://github.com/hluk/CopyQ/releases).
- OBS packages

      utils/download_obs_packages.sh 3.3.1 1.1

Upload packages and binaries to:
- [github](https://github.com/hluk/CopyQ/releases)
- [sourceforge](https://sourceforge.net/projects/copyq/files/)

Update Homebrew package for OS X.

    brew install vitorgalvao/tiny-scripts/cask-repair
    cask upgrade cask-repair
    cask-repair copyq

Update [flathub package](https://github.com/flathub/com.github.hluk.copyq):
- update "tag" and "commit" in "com.github.hluk.copyq.json" file,
- push to fork,
- create pull request,
- add comment "bot, build",
- [verify the build](https://flathub.org/builds/#/),
- merge the changes if build is OK.

Write release announcement to [CopyQ group](https://groups.google.com/forum/#!forum/copyq).
