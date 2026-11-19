#!/bin/sh
# TPP06 carga de usuario persistente (modo TRANSPARENTE: mantiene app Tuya).
LOG=/tuya/data/custom/boot.log
echo "boot $(date '+%Y-%m-%d %H:%M:%S') uptime=$(cut -d' ' -f1 /proc/uptime)" >> "$LOG"
CUSTOM=/tuya/data/custom

# --- SSH persistente (dropbear) ---
SSHDIR="$CUSTOM/ssh"
if ! pidof dropbear >/dev/null 2>&1; then
	"$SSHDIR/dropbear" -r "$SSHDIR/keys/dropbear_ed25519_host_key" \
		-D "$SSHDIR" -p 22 -P /var/run/dropbear.pid \
		</dev/null >/tmp/dropbear.log 2>&1
	echo "dropbear lanzado" >> "$LOG"
fi

# --- Agente MQTT (gestión del panel + HA discovery) ---
if [ -x "$CUSTOM/tpp06_agent" ] && ! pidof tpp06_agent >/dev/null 2>&1; then
	nohup "$CUSTOM/tpp06_agent" "$CUSTOM/tpp06.conf" </dev/null >/tmp/agent.log 2>&1 &
	echo "tpp06_agent lanzado" >> "$LOG"
fi

# --- Mantener la app Tuya (hasta tener la UI propia) ---
/etc/init.d/S99tuya-app start
