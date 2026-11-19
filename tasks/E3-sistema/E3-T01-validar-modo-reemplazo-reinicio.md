---
id: E3-T01
epic: E3 · Sistema
title: Validar modo reemplazo con reinicio real
status: blocked
estimate: S
optional: false
dependsOn: [E2-T01]
---

## Bloqueo (2026-11-18)
PRIMER intento de validación = **FALLO que dejó el panel inaccesible**: el modo reemplazo NO
arranca Tuya, y era Tuya quien asociaba la **WiFi** (`wpa_supplicant`+DHCP) y ataba el **gadget
ADB**. Sin ella: sin red, sin ADB, parado en el logo MOES. Recuperación = **UART** (confirmado,
ver `docs/RECUPERACION-UART.md`); el usuario pidió un adaptador FT232RNL 3.3 V y se espera a que
llegue. Desbloqueo: (1) recuperar acceso por UART; (2) correr `scripts/diag_arranque.sh`; (3)
terminar y desplegar `boot.replace.safe.sh` (WiFi+ADB propios + fallback a Tuya); (4) re-probar
el reinicio **con el UART puesto** como red de seguridad. Ver [[tpp06-modo-reemplazo-wifi]].

## Objetivo
Confirmar que, tras un reinicio real, el panel arranca la UI LVGL de forma estable y, pase lo que
pase, **mantiene la red (SSH/ADB) viva** (si no hay red propia, cae a Tuya; nunca se queda sin red).

## Contexto
El `boot.sh` "modo reemplazo" original (dropbear + supervisor `tpp06_lvgl`, sin Tuya) es el que
falló. El sustituto es `boot.replace.safe.sh`: levanta WiFi+ADB propios y solo mata Tuya/lanza la
UI si la red propia está OK; si no, fallback a Tuya. Ver [[tpp06-lvgl-ui]],
[[tpp06-persistencia-overlay]], [[tpp06-ssh-persistente]], [[tpp06-modo-reemplazo-wifi]].

## Criterios de aceptación
- [ ] Tras `reboot`, el panel tiene **red propia** (WiFi asociada + IP) y **SSH/ADB** accesibles,
      SIN depender de Tuya.
- [ ] Con red propia OK, arranca `tpp06_lvgl` (UI en pantalla) y el supervisor lo revive si muere.
- [ ] **Fallback probado**: si la WiFi propia no asocia, el arranque cae a Tuya (panel usable +
      remoto) en vez de quedarse en el logo.
- [ ] Relés/botones físicos funcionan tras el arranque.
- [ ] Recuperación documentada por **UART** (`docs/RECUPERACION-UART.md`) y por `boot.sh.tuya.bak`.

## Notas técnicas
GOTCHA: `tuya_monitor` reinicia Tuya; matar por PID (busybox sin pkill/pgrep). La pantalla necesita
Tuya MATADA (no congelada). CLAVE: nunca matar Tuya hasta tener **red propia confirmada** (si no,
se pierde el acceso). Probar SIEMPRE con el UART conectado. Logs: `boot.log`, `/tmp/ui.log`.

## Testing
Reinicio físico **con UART puesto**; comprobar IP en wlan0 + SSH; matar la UI por SSH y ver que
revive; forzar fallo de WiFi (p.ej. wifi.conf inválido) y ver que cae a Tuya.
