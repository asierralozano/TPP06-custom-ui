---
id: E5-T02
epic: E5 · HA
title: Detalles (long-press) por tipo de entidad + arreglar covers
status: todo
estimate: L
optional: false
dependsOn: []
---

## Objetivo
Que la pantalla de detalle (long-press) sea específica y completa por tipo de entidad, recreando
lo que ofrece la app/firmware de Tuya. En particular, **arreglar las covers/persianas** (ahora no
funcionan bien) y añadir más tipos (sensor, clima/termostato, media_player, escenas…).

## Contexto
El long-press abre una pantalla de detalle a pantalla completa (con atrás). Para luces hay
on/off + brillo; para covers se añadieron botones abrir/parar/cerrar **pero no funcionan
correctamente** (revisar servicios `cover.*`, estado open/closed/position, y el toggle del tile).
Ver [[tpp06-lvgl-ui]], [[tpp06-ui-editor-vision]].

## Criterios de aceptación
- [ ] **Covers**: abrir/parar/cerrar funcionan contra HA; el tile refleja abierto/cerrado; si hay
      posición, un slider de posición.
- [ ] **Luces**: on/off + brillo (+ color/temperatura si la entidad lo soporta).
- [ ] Al menos un tipo nuevo con detalle propio (p.ej. termostato: temperatura objetivo; o
      media_player: play/pausa/volumen).
- [ ] Estados en vivo reflejados; diseño coherente y "tipo Tuya".

## Notas técnicas
Ampliar `ha_ws` si hace falta (atributos: position, hvac, volume…). Servicios por dominio
(`cover.open_cover/close_cover/stop_cover/set_cover_position`, `climate.set_temperature`,
`media_player.*`). Revisar por qué las persianas no responden (¿dominio/servicio/estado?).

## Testing
Prueba física con una persiana real y una luz regulable; verificar cada control contra HA.
