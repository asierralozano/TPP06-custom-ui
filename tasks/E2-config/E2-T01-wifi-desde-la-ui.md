---
id: E2-T01
epic: E2 · Config
title: WiFi desde la UI: escanear + conectar (wpa) con teclado
status: todo
estimate: L
optional: false
dependsOn: []
---

## Objetivo
Configurar la WiFi del panel desde la propia pantalla: listar redes, elegir una e introducir la
clave con el teclado en pantalla, y conectar de forma persistente.

## Contexto
Clave para el instalador plug-and-play (el panel tiene que poder conectarse a la WiFi del cliente
sin cable). El panel usa wpa_supplicant. Hoy la pantalla de WiFi en Ajustes es solo informativa
(SSID + IP). Ver [[tpp06-lvgl-ui]], [[tpp06-vision-producto]].

## Criterios de aceptación
- [ ] Ajustes → WiFi escanea y lista las redes disponibles (con intensidad).
- [ ] Al elegir una, teclado en pantalla para la contraseña.
- [ ] Conecta y la configuración **persiste** tras reinicio (wpa_supplicant.conf).
- [ ] Muestra estado de conexión (conectado/IP/errores).

## Notas técnicas
Interactuar con `wpa_cli` (scan, scan_results, add_network, set_network ssid/psk, enable_network,
save_config) vía `popen`/comandos; o escribir `wpa_supplicant.conf` y relanzar. Comprobar qué hay
en el panel (`wpa_cli`, ruta del conf). El teclado ya existe (lv_keyboard).

## Testing
Prueba física: conectar a una red de prueba, reiniciar y verificar que reconecta solo.
