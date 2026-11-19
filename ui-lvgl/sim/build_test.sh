#!/bin/sh
set -e
cd "$(dirname "$0")/.."
SRCS=$(find lvgl/src -name "*.c")
cc -O1 -g -DLV_CONF_INCLUDE_SIMPLE -I. -Ilvgl \
   -o sim/test_font sim/test_font.c fonts.c $SRCS -lm
./sim/test_font
