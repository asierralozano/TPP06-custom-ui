#!/bin/sh
set -e
cd "$(dirname "$0")/.."
SRCS=$(find lvgl/src -name "*.c")
echo "compilando sim (LVGL + UI) ..."
cc -O1 -DLV_CONF_INCLUDE_SIMPLE -I. -Ilvgl \
   -o sim/sim_render sim/sim_render.c fonts.c assets.c \
   tuya_home.c tuya_cards.c tuya_detail.c tuya_screensaver.c tuya_settings.c tuya_control_center.c tuya_add_entity.c tuya_scenes.c tuya_media.c \
   $SRCS -lm
echo "renderizando ..."
ASSET_DIR="$PWD/../tuya-fw/img" ./sim/sim_render
for n in home devices light_detail cover_detail screensaver settings control_center add_entity thermostat_detail devices_mix scenes media_detail; do sips -s format png sim/$n.bmp --out sim/$n.png >/dev/null 2>&1 || true; done
ls sim/*.png 2>/dev/null
