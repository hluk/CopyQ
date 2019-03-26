#!/bin/bash
exec "${LUPDATE:-lupdate-qt5}" src/ plugins/*/*.{cpp,h,ui} -ts translations/*.ts
