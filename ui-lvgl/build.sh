#!/bin/sh
# Compila tpp06_lvgl (LVGL + main) para ARMv5 static con la toolchain Debian en Docker.
set -e
cd "$(dirname "$0")"
docker run --rm -v "$PWD":/src -w /src debian:bookworm-slim bash -c '
  set -e
  apt-get update -qq >/dev/null 2>&1
  apt-get install -y -qq gcc-arm-linux-gnueabi >/dev/null 2>&1
  SRCS=$(find lvgl/src -name "*.c")
  echo "compilando $(echo "$SRCS" | wc -l) ficheros de LVGL + main.c ..."
  arm-linux-gnueabi-gcc -static -Os -march=armv5te -mfloat-abi=soft -marm \
    -DLV_CONF_INCLUDE_SIMPLE -I. -Ilvgl \
    -o tpp06_lvgl main.c ha_ws.c fonts.c $SRCS -lm
  arm-linux-gnueabi-strip tpp06_lvgl
'
ls -la tpp06_lvgl
file tpp06_lvgl 2>/dev/null || true
