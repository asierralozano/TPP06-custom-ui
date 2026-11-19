#!/bin/sh
# ============================================================================
#  TPP06 — Instalador de persistencia por overlay (REEMPLAZO de la app Tuya)
# ============================================================================
#  EJECUTAR EN LA CONSOLA ROOT DEL PANEL (no en el Mac).
#
#  Qué hace, y por qué es seguro:
#   - Escribe /tuya/data/etc/init.d/rc.local (capa superior del overlay de /etc).
#     Es una COPIA EXACTA del rc.local de fabrica, con un solo cambio al final:
#     en vez de arrancar la app Tuya (S99tuya-app), arranca nuestra carga
#     /tuya/data/custom/boot.sh.
#   - Mantiene TODOS los insmod de drivers (pantalla, tactil, wifi, BT, watchdog)
#     y MANTIENE el arranque de adbd ANTES de nuestra carga: si boot.sh falla,
#     el panel sigue vivo y accesible por ADB para recuperar.
#   - Es 100% reversible:  rm /tuya/data/etc/init.d/rc.local  -> vuelve el stock.
#
#  Derivado del firmware OTA 1.2.3 (rootfs /etc/init.d/rc.local verbatim).
#  Requiere: /tuya/data montado (YAFFS2 persistente) y overlay de /etc activo.
# ============================================================================
set -e

DATA_ETC=/tuya/data/etc/init.d
CUSTOM=/tuya/data/custom
RCLOCAL="$DATA_ETC/rc.local"

echo "== Comprobaciones previas =="
# 1) /tuya/data debe ser el YAFFS2 persistente, no un tmpfs
if ! grep -q " /tuya/data " /proc/mounts ; then
    echo "ERROR: /tuya/data no esta montado. Aborto."; exit 1
fi
mount | grep -q "overlay on /etc" || mount | grep -q " /etc " || {
    echo "AVISO: no veo el overlay de /etc en 'mount'. Continuo, pero revisa rc.mount."; }

mkdir -p "$DATA_ETC" "$CUSTOM"

echo "== Escribiendo la carga de usuario: $CUSTOM/boot.sh =="
# boot.sh: NUESTRA carga. De momento arranca SSH (dropbear) como acceso
# persistente. La UI nativa se anadira aqui cuando el binario este listo.
cat > "$CUSTOM/boot.sh" <<'BOOT'
#!/bin/sh
# Carga de usuario TPP06 (persistente). Se lanza desde rc.local (overlay).
# No debe bloquear el arranque: todo en segundo plano.
LOG=/tmp/tpp06_custom.log
echo "[`date`] boot.sh start" >> "$LOG"

# --- SSH persistente (dropbear) ---
# Coloca los binarios en /tuya/data/custom/ssh/ y las claves de host en
# /tuya/data/custom/ssh/keys/ (persistentes). Descomenta cuando esten subidos:
# DROPBEAR_DIR=/tuya/data/custom/ssh
# if [ -x "$DROPBEAR_DIR/dropbear" ]; then
#     mkdir -p "$DROPBEAR_DIR/keys"
#     [ -f "$DROPBEAR_DIR/keys/dropbear_ed25519_host_key" ] || \
#         "$DROPBEAR_DIR/dropbearkey" -t ed25519 -f "$DROPBEAR_DIR/keys/dropbear_ed25519_host_key" >> "$LOG" 2>&1
#     "$DROPBEAR_DIR/dropbear" -r "$DROPBEAR_DIR/keys/dropbear_ed25519_host_key" \
#         -p 22 -P /tmp/dropbear.pid >> "$LOG" 2>&1 &
#     echo "[`date`] dropbear lanzado" >> "$LOG"
# fi

# --- UI nativa propia (reemplazo de la app Tuya) ---
# UI=/tuya/data/custom/tpp06ui
# if [ -x "$UI" ]; then
#     "$UI" >> "$LOG" 2>&1 &
#     echo "[`date`] UI lanzada" >> "$LOG"
# fi

echo "[`date`] boot.sh end" >> "$LOG"
BOOT
chmod 755 "$CUSTOM/boot.sh"

echo "== Escribiendo el overlay rc.local: $RCLOCAL =="
# rc.local = stock VERBATIM, salvo la ultima linea (tuya-app -> boot.sh).
cat > "$RCLOCAL" <<'RCL'
#!/bin/sh

ifconfig lo 127.0.0.1

# Set Global Environment
source /etc/profile

insmod /usr/modules/ak_ion.ko
insmod /usr/modules/ak_gui.ko
insmod /usr/modules/ak_uio.ko
modprobe ak_pwm_char.ko
modprobe pwm_bl
modprobe ak_fb.ko lcdc_wait_mode=1
modprobe srgb_lcd.ko

# butten-led
insmod /usr/modules/ak_leds.ko

# adbd  (CANAL DE RECUPERACION: se mantiene siempre)
modprobe usb_f_fs.ko
modprobe usb_f_mass_storage
modprobe usbcore
insmod /usr/modules/ak_udc.ko
/etc/init.d/S50adbd start

# touch panel
insmod /usr/modules/ak_i2c.ko combined_format=1
modprobe gt1

# wifi
modprobe cfg80211
modprobe mmc_core
insmod /usr/modules/ak_mci.ko
echo 0 > /sys/class/gpio/WIFI_PWREN/value
insmod /usr/modules/rtl8723ds.ko
ifconfig wlan0 up

#bluetooth
echo 0 > /sys/class/gpio/BT_RST_N/value
sleep 1
echo 1 > /sys/class/gpio/BT_RST_N/value
insmod /lib/modules/hci_uart.ko
rtk_hciattach -n -s 115200 /dev/ttySAK1 rtk_h5 &

# udev
/etc/init.d/S10udev start

if [  -e /etc/init.d/S03zramctl ]; then
	/etc/init.d/S03zramctl start
fi

#ak_watchdog timeout:20  keepalive:10
/bin/ak_watchdog 20 10 &

# ====== CAMBIO: en vez de la app Tuya, nuestra carga de usuario ======
# (stock era:  /etc/init.d/S99tuya-app start )
if [ -x /tuya/data/custom/boot.sh ]; then
	/tuya/data/custom/boot.sh &
else
	echo "boot.sh no encontrado; arranco app Tuya de fabrica como respaldo"
	/etc/init.d/S99tuya-app start
fi
RCL
chmod 755 "$RCLOCAL"

echo "== Verificacion =="
echo "  rc.local overlay : $RCLOCAL ($(wc -c < "$RCLOCAL") bytes)"
echo "  boot.sh          : $CUSTOM/boot.sh ($(wc -c < "$CUSTOM/boot.sh") bytes)"
echo ""
echo "LISTO. En el proximo reinicio se cargaran los drivers y luego boot.sh"
echo "en vez de la app Tuya. Para revertir:  rm $RCLOCAL  (y reinicia)."
echo "IMPORTANTE: haz primero el backup de la NAND (backup_nand.sh)."
