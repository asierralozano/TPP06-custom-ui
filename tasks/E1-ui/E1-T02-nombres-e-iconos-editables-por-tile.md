---
id: E1-T02
epic: E1 · UI
title: Nombres editables e iconos personalizables por tile
status: todo
estimate: M
optional: false
dependsOn: []
---

## Objetivo
Permitir, desde la propia UI, cambiar el **nombre** de un tile (relé o entidad) y elegir su
**icono**, sin tocar el fichero de config a mano.

## Contexto
El layout ya se persiste en `tpp06_ui.conf` (campos `relay=`/`widget=` con label). Falta una
pantalla de edición del tile (en modo editar, al mantener pulsado o con un lápiz en el tile) con
un campo de texto (teclado en pantalla) para el nombre y un selector de icono. Ver [[tpp06-lvgl-ui]],
[[tpp06-ui-editor-vision]].

## Criterios de aceptación
- [ ] En modo editar, se puede abrir la edición de un tile concreto.
- [ ] Cambiar el nombre con teclado en pantalla y guardar → persiste en `tpp06_ui.conf` y se ve.
- [ ] Elegir un icono de una lista (depende de E1-T01) → persiste (nuevo campo en config) y se ve.
- [ ] Sobrevive a reinicio.

## Notas técnicas
Reutilizar el patrón de `lv_textarea` + `lv_keyboard` ya usado en la edición de Home Assistant.
Añadir campo opcional de icono al formato de `widget=`/`relay=` (compat hacia atrás).

## Testing
Prueba física: renombrar un relé, reiniciar la app y comprobar que mantiene el nombre/icono.
