#!/bin/bash
lupdate_args=(
    src/
    plugins/*/*.{cpp,h,ui}
    -ts
    ${1:-translations/*.ts}
)
exec "${LUPDATE:-lupdate-qt6}" "${lupdate_args[@]}"
