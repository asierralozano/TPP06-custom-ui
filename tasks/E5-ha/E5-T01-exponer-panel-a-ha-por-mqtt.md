---
id: E5-T01
epic: E5 · HA
title: Exponer el panel a HA por MQTT en modo reemplazo
status: todo
estimate: M
optional: false
dependsOn: [E3-T01]
---

## Objetivo
Que, en modo reemplazo, el panel siga apareciendo en HA (pantalla on/off, brillo, uptime, wifi…)
por MQTT Discovery, como hacía el agente.

## Contexto
El agente `tpp06_agent` (MQTT discovery + control del panel) NO corre en modo reemplazo porque
compartía el control del backlight con la UI LVGL y peleaban. Hay que decidir: integrar el MQTT
dentro de `tpp06_lvgl` (reutilizando la lógica del agente) o hacerlos convivir sin conflicto. Ver
[[tpp06-agente-mqtt]], [[tpp06-lvgl-ui]].

## Criterios de aceptación
- [ ] En modo reemplazo, el panel aparece en HA como device con: luz Pantalla (on/off+brillo),
      sensores (uptime/wifi/mem/ip) y botón reiniciar.
- [ ] El control del backlight no entra en conflicto con la inactividad/auto-apagado de la UI.
- [ ] Config MQTT editable (ligado a E2-T02).
- [ ] Persiste y sobrevive a reinicio.

## Notas técnicas
Opción A: portar el cliente MQTT del agente a un hilo/loop dentro de `tpp06_lvgl` (un solo proceso
controla backlight). Opción B: mantener el agente pero coordinar el backlight con la UI. Preferible A.

## Testing
`mosquitto_sub -t 'tpp06/#'` y ver el device en HA; cambiar brillo desde HA y en el panel.
