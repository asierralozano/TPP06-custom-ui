---
id: E2-T02
epic: E2 · Config
title: Editar MQTT del agente (tpp06.conf) con teclado
status: todo
estimate: M
optional: false
dependsOn: []
---

## Objetivo
Permitir editar la configuración MQTT (broker host/puerto/usuario/contraseña) desde Ajustes, con
teclado en pantalla, y que se aplique.

## Contexto
El MQTT lo usa el agente `tpp06_agent` (config en `/tuya/data/custom/tpp06.conf`). Hoy en Ajustes
la pantalla MQTT es informativa. En modo reemplazo el agente no corre (ver E5-T01), así que esta
tarea va ligada a decidir cómo convive/integra el MQTT. Ver [[tpp06-agente-mqtt]], [[tpp06-lvgl-ui]].

## Criterios de aceptación
- [ ] Ajustes → MQTT con campos editables (host, puerto, usuario, contraseña) + teclado.
- [ ] Guardar escribe `tpp06.conf` (atómico) y reinicia/avisa al componente MQTT.
- [ ] Persiste tras reinicio.

## Notas técnicas
Reutilizar el patrón `lv_textarea`+`lv_keyboard` de la edición de HA. Escritura atómica (tmp+rename)
como `save_config`. La contraseña no se registra en logs. Coordinar con E5-T01 (quién corre el MQTT).

## Testing
Editar el broker, verificar que el panel publica/recibe por MQTT en HA (mosquitto_sub).
