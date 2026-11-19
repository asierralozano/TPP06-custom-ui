#!/bin/sh
set -e
cd "$(dirname "$0")/.."
SRCS=$(find lvgl/src -name "*.c")
cc -O0 -g -DLV_CONF_INCLUDE_SIMPLE -I. -Ilvgl -o sim/sim_render sim/sim_render.c fonts.c tuya_home.c $SRCS -lm
