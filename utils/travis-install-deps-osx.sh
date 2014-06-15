#!/usr/bin/env bash

set -e -x

brew install qt5 python
pip install cpp-coveralls

# Install pybojc from mercurial repo instead of using PyPi due to
# https://bitbucket.org/ronaldoussoren/pyobjc/issue/86
# Install from the tar.gz to make the download faster - temporary until
# 3.x is released
(
    set -e
    dir=$(mktemp -d -t pyobjc-install.XXXXXX)
    trap "rm -r ${dir}" EXIT
    cd ${dir}
    mkdir pyobjc
    curl -o - https://bitbucket.org/ronaldoussoren/pyobjc/get/pyobjc-3.0.x.tar.gz | tar -xz --strip 1 -C pyobjc

    for module in core framework-Cocoa framework-Quartz
    do
        echo installing ${module}
        ( cd pyobjc/pyobjc-${module}/ && python setup.py install )
    done
)

pip install dmgbuild
