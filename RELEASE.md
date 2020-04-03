This is step-by-step description on how to release new version of CopyQ.

# Verify the Builds

- [copyq-beta Ubuntu package](https://launchpad.net/~hluk/+archive/ubuntu/copyq-beta)
- [copyq-beta on OBS](https://build.opensuse.org/package/show/home:lukho:copyq-beta/CopyQ-Qt5-beta)
- [Windows builds](https://ci.appveyor.com/project/hluk/copyq)

# Update Version and Changelog

Update CHANGES file.

Bump version.

    utils/bump_version.sh 3.3.1

Verify and push the changes.

    for r in origin gitlab bitbucket; do git push --follow-tags $r master || break; done

# Build Packages

Upload source files for [copyq Ubuntu package](https://launchpad.net/~hluk/+archive/ubuntu/copyq).

    git checkout v3.3.1
    utils/debian/create_source_packages.sh
    dput ppa:hluk/copyq ../copyq_3.3.1~*.changes

Build on [OBS](https://build.opensuse.org/package/show/home:lukho:copyq/CopyQ-Qt5).

    osc co home:lukho:copyq
    cd home:lukho:copyq/CopyQ-Qt5
    ./create_beta_package.sh
    $EDITOR debian.changelog
    osc commit

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
- binary for OS X from [github](https://github.com/hluk/CopyQ/releases)
- source package from [github](https://github.com/hluk/CopyQ/releases)
- OBS packages

      utils/download_obs_packages.sh 3.3.1 1.1

Create [release on github](https://github.com/hluk/CopyQ/releases) for the new version tag.

Upload packages and binaries to:

- [github](https://github.com/hluk/CopyQ/releases)
- [sourceforge](https://sourceforge.net/projects/copyq/files/)
- [fosshub](https://www.fosshub.com/CopyQ.html)

        ./utils/fosshub.py 3.3.1 $TOKEN

Update Homebrew package for OS X.

    brew install vitorgalvao/tiny-scripts/cask-repair
    brew upgrade cask-repair
    cask-repair copyq

Write release announcement to [CopyQ group](https://groups.google.com/forum/#!forum/copyq).
