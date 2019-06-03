#!/bin/bash
exec "${LUPDATE:-lupdate-qt5}" src/ plugins/*/*.{cpp,h,ui} -ts ${1:-translations/*.ts}
