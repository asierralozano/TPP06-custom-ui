#!/bin/sh
# diag_arranque.sh — recoge en el panel TPP06 todo lo necesario para dejar el
# modo reemplazo SEGURO (WiFi + ADB + SSH propios, con fallback a Tuya).
# Ejecutar en el panel (SSH/ADB/UART) tras recuperar acceso:
#   sh diag_arranque.sh 2>&1 | tee /tmp/diag.txt
# y traerse /tmp/diag.txt.  No imprime secretos (ni PSK ni tokens).

sep(){ echo; echo "########## $* ##########"; }

sep "SISTEMA"
uname -a; cat /proc/uptime; echo "---"; cat /etc/os-release 2>/dev/null

sep "LOGS DEL ARRANQUE FALLIDO (por qué la UI no pintó)"
echo "-- boot.log --"; tail -n 40 /tuya/data/custom/boot.log 2>/dev/null
echo "-- ui.log --";   tail -n 40 /tmp/ui.log 2>/dev/null
echo "-- dmesg (fb/wifi/usb/gadget) --"; dmesg 2>/dev/null | grep -iE "fb|wlan|rtl8723|dwc|udc|gadget|adb|ffs|panic|oops" | tail -40

sep "WIFI: binarios y config (SIN imprimir la PSK)"
for b in wpa_supplicant wpa_cli udhcpc dhcpcd iw iwconfig iwlist; do
  p=$(command -v $b 2>/dev/null); echo "$b: ${p:-NO}"
done
echo "-- interfaz wlan0 --"; ifconfig wlan0 2>/dev/null; iwconfig wlan0 2>/dev/null
echo "-- drivers 802.11 cargados --"; lsmod 2>/dev/null | grep -iE "cfg80211|rtl8723|mac80211"
echo "-- posibles wpa_supplicant.conf (solo rutas y SSID, NO psk) --"
for f in /etc/wpa_supplicant.conf /tmp/wpa_supplicant.conf /tuya/data/*wpa* /var/*wpa* /tuya/*wpa*; do
  [ -f "$f" ] && { echo "== $f =="; grep -iE "ssid|key_mgmt|proto|scan" "$f" | grep -vi "psk=" ; }
done 2>/dev/null
echo "-- cómo arranca Tuya la wifi (buscar en sus scripts/binarios, solo referencias) --"
grep -rslE "wpa_supplicant|udhcpc|WIFI_PWREN" /etc/init.d /tuya 2>/dev/null | head

sep "ADB / GADGET USB (por qué no enumera sin Tuya)"
echo "-- S50adbd --"; cat /etc/init.d/S50adbd 2>/dev/null
echo "-- /sys/class/udc --"; ls /sys/class/udc 2>/dev/null
echo "-- configfs gadget --"; ls -R /sys/kernel/config/usb_gadget 2>/dev/null | head -40
echo "-- UDC atado? --"; cat /sys/kernel/config/usb_gadget/*/UDC 2>/dev/null
echo "-- android_usb (esquema viejo) --"; ls /sys/class/android_usb/android0 2>/dev/null; cat /sys/class/android_usb/android0/enable 2>/dev/null
echo "-- nvram adb --"; nvram show 2>/dev/null | grep -i adb
echo "-- adbd corriendo? --"; ps 2>/dev/null | grep -E "[a]dbd"

sep "PROCESOS TUYA (para matar solo el display, no la red)"
ps 2>/dev/null | grep -iE "[v]oice_control|[t]uya|[m]onitor"
echo "-- S99tuya-app --"; head -40 /etc/init.d/S99tuya-app 2>/dev/null

sep "NUESTRO CUSTOM"
ls -la /tuya/data/custom/ 2>/dev/null
echo "-- ssh/dropbear --"; ls -la /tuya/data/custom/ssh 2>/dev/null
echo "-- tpp06_lvgl presente/ejecutable --"; ls -la /tuya/data/custom/tpp06_lvgl 2>/dev/null

sep "PARTICION DATA (confirmar mtd/fs para recuperación UART)"
cat /proc/mtd 2>/dev/null
mount 2>/dev/null | grep -iE "tuya|data|yaffs|jffs|ubi"

sep "FIN"
echo "diag completo. Trae /tmp/diag.txt"
