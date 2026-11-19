#!/bin/sh
# ============================================================================
# TPP06 — arranque INDEPENDIENTE y SEGURO (modo reemplazo)
# ----------------------------------------------------------------------------
# Principio rector: NUNCA dejar el panel sin RED. Mientras la red (SSH/ADB) esté
# viva, cualquier fallo de la UI se arregla en remoto. El desastre anterior fue
# que la red dependía de Tuya; aquí la levantamos nosotros, y si no podemos,
# devolvemos el control a Tuya (fallback) en vez de quedarnos en el logo.
#
# Reutilizable: esta misma lógica vale como overlay (ahora) y como init del
# futuro firmware flasheado (solo cambia quién la invoca).
#
# >>> Marcadores [CONFIRM] = ajustar con la salida de scripts/diag_arranque.sh
#     antes de darlo por bueno. PROBAR siempre con el UART puesto.
# ============================================================================
CUSTOM=/tuya/data/custom
LOG=$CUSTOM/boot.log
log(){ echo "$(date '+%F %T') $*" >> "$LOG"; }
log "=== boot replace-safe uptime=$(cut -d' ' -f1 /proc/uptime) ==="

kill_by(){ for p in $(ps 2>/dev/null | grep -E "$1" | grep -v grep | awk '{print $1}'); do kill -9 "$p" 2>/dev/null; done; }

start_tuya(){
    log "FALLBACK -> arranco Tuya (recupera red/ADB a su manera)"
    kill_by '[w]pa_supplicant'     # soltar wlan0 para no pelear con Tuya
    kill_by '[u]dhcpc'
    /etc/init.d/S99tuya-app start
}

# --- 1. ADB: asegurar que el gadget enumera (sin depender de Tuya) ----------
# [CONFIRM] El método real (configfs vs android_usb) lo da el diagnóstico.
# rc.local ya hace modprobe usb_f_fs + ak_udc + S50adbd; si no ata el UDC, lo atamos.
UDC=$(ls /sys/class/udc 2>/dev/null | head -1)
GADGET=$(ls -d /sys/kernel/config/usb_gadget/* 2>/dev/null | head -1)
if [ -n "$GADGET" ] && [ -n "$UDC" ]; then
    if [ -z "$(cat "$GADGET/UDC" 2>/dev/null)" ]; then
        echo "$UDC" > "$GADGET/UDC" 2>/dev/null && log "ADB: UDC atado ($UDC)"
    fi
fi

# --- 2. WiFi propia (wpa_supplicant + DHCP) ---------------------------------
# wifi.conf = fichero wpa_supplicant (ssid + psk). NUNCA en git/memoria.
# [CONFIRM] ruta/driver de wpa_supplicant; si no existe en rootfs, bundle estático.
WIFI_OK=0
if [ -f "$CUSTOM/wifi.conf" ] && command -v wpa_supplicant >/dev/null 2>&1; then
    ifconfig wlan0 up 2>/dev/null
    kill_by '[w]pa_supplicant'
    wpa_supplicant -B -i wlan0 -c "$CUSTOM/wifi.conf" -D nl80211,wext >> "$LOG" 2>&1
    i=0
    while [ $i -lt 25 ]; do
        if udhcpc -i wlan0 -n -q -t 2 -T 2 >> "$LOG" 2>&1; then WIFI_OK=1; break; fi
        sleep 1; i=$((i+1))
    done
fi
if [ "$WIFI_OK" = 1 ]; then
    log "WiFi propia OK ($(ifconfig wlan0 2>/dev/null | grep -o 'inet addr:[0-9.]*'))"
else
    log "WiFi propia NO disponible (falta wifi.conf/wpa_supplicant o no asoció)"
fi

# --- 3. SSH (dropbear): canal de recuperación, escucha aunque la IP tarde ----
if [ -x "$CUSTOM/ssh/dropbear" ] && ! pidof dropbear >/dev/null 2>&1; then
    "$CUSTOM/ssh/dropbear" -r "$CUSTOM/ssh/keys/dropbear_ed25519_host_key" \
        -p 22 -P /var/run/dropbear.pid </dev/null >/tmp/dropbear.log 2>&1
    log "dropbear up"
fi

# --- 4. Si NO hay red propia -> fallback Tuya y no tocar el display ----------
if [ "$WIFI_OK" != 1 ]; then
    start_tuya
    log "sin red propia: dejo Tuya al mando (panel usable + remoto). Fin."
    exit 0
fi

# --- 5. Agente MQTT opcional (gestión del panel en HA) ----------------------
if [ -x "$CUSTOM/tpp06_agent" ] && ! pidof tpp06_agent >/dev/null 2>&1; then
    nohup "$CUSTOM/tpp06_agent" "$CUSTOM/tpp06.conf" </dev/null >/tmp/agent.log 2>&1 &
    log "tpp06_agent up"
fi

# --- 6. Supervisor de la UI con fallback -------------------------------------
# Red propia OK => podemos arriesgar la UI: si crashea en bucle, devolvemos a Tuya.
# Aunque la UI se cuelgue sin salir, SEGUIMOS con SSH/ADB -> arreglable en remoto.
if [ -x "$CUSTOM/tpp06_lvgl" ]; then
    kill_by '[v]oice_control'   # [CONFIRM] nombre exacto del display de Tuya
    kill_by '[t]uya_monitor'
    (
        crashes=0
        while true; do
            t0=$(cut -d. -f1 /proc/uptime)
            "$CUSTOM/tpp06_lvgl" >> /tmp/ui.log 2>&1
            t1=$(cut -d. -f1 /proc/uptime)
            if [ $((t1 - t0)) -lt 10 ]; then crashes=$((crashes + 1)); else crashes=0; fi
            log "tpp06_lvgl salió (vida=$((t1 - t0))s crashes=$crashes)"
            if [ "$crashes" -ge 3 ]; then
                log "UI inestable (3 crashes rápidos) -> fallback Tuya"
                start_tuya
                break
            fi
            sleep 2
        done
    ) &
    log "supervisor UI lanzado"
else
    log "tpp06_lvgl ausente -> fallback Tuya"
    start_tuya
fi
