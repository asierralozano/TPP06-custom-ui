---
id: E1-T04
epic: E1 · UI
title: Mejorar/clonar el salvapantallas del firmware
status: todo
estimate: S
optional: true
dependsOn: []
---

## Objetivo
(OPCIONAL) Mejorar el salvapantallas de inactividad; idealmente recrear el que trae el firmware
Tuya (reloj con fondo bonito), o pulir el nuestro.

## Contexto
Ya tenemos un salvapantallas propio (reloj) que entra a los 40 s y apaga la pantalla a los 90 s
(ver `manage_idle` en `ui-lvgl/main.c`). El del firmware tiene un estilo concreto; clonarlo
requeriría extraer sus assets del rootfs. Es un "nice to have". Ver [[tpp06-lvgl-ui]].

## Criterios de aceptación
- [ ] Salvapantallas con reloj grande + fecha sobre un fondo agradable (o assets del firmware).
- [ ] Tiempos de inactividad configurables desde Ajustes.
- [ ] El táctil despierta correctamente a la pantalla anterior.

## Notas técnicas
Si se clona el de Tuya: buscar los assets en el squashfs del user/oem partition. Si no, animación
suave o segundos opcionales en el reloj. No bloquea nada.

## Testing
Dejar el panel inactivo y comprobar el salvapantallas y el despertar.
