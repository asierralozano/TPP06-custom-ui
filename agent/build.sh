#!/bin/sh
# Compila tpp06_agent y tpp06_ui para ARMv5 (static) con la toolchain de Debian en Docker.
set -e
cd "$(dirname "$0")"
docker run --rm -v "$PWD":/src -w /src debian:bookworm-slim bash -c '
  set -e
  apt-get update -qq >/dev/null 2>&1
  apt-get install -y -qq gcc-arm-linux-gnueabi >/dev/null 2>&1
  CC=arm-linux-gnueabi-gcc
  FLAGS="-static -Os -march=armv5te -mfloat-abi=soft -marm -Wall"
  $CC $FLAGS -o tpp06_agent tpp06_agent.c
  arm-linux-gnueabi-strip tpp06_agent
  $CC $FLAGS -o tpp06_ui tpp06_ui.c ha_ws.c
  arm-linux-gnueabi-strip tpp06_ui
'
ls -la tpp06_agent tpp06_ui
file tpp06_ui 2>/dev/null || true
