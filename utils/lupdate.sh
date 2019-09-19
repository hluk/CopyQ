#!/bin/bash
lupdate_args=(
    -no-obsolete
    src/
    plugins/*/*.{cpp,h,ui}
    -ts
    ${1:-translations/*.ts}
)
exec "${LUPDATE:-lupdate-qt5}" "${lupdate_args[@]}"
