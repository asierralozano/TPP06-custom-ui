#!/bin/sh
set -e
cd "$(dirname "$0")/.."
SRCS=$(find lvgl/src -name "*.c")
echo "compilando sim_nav (router + UI) ..."
cc -O1 -DLV_CONF_INCLUDE_SIMPLE -I. -Ilvgl \
   -o sim/sim_nav sim/sim_nav.c ui_nav.c tuya_ui.c fonts.c assets.c \
   tuya_home.c tuya_cards.c tuya_detail.c tuya_screensaver.c tuya_settings.c \
   tuya_control_center.c tuya_add_entity.c tuya_scenes.c tuya_media.c \
   $SRCS -lm
echo "ejecutando navegación ..."
ASSET_DIR="$PWD/../tuya-fw/img" ./sim/sim_nav
for n in nav_1_home nav_2_devices nav_3_detail nav_4_back nav_5_control; do
  sips -s format png sim/$n.bmp --out sim/$n.png >/dev/null 2>&1 || true
done
ls sim/nav_*.png 2>/dev/null
