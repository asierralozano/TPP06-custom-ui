---
id: E1-T01
epic: E1 · UI
title: Iconos MDI por tipo de entidad en los tiles
status: todo
estimate: M
optional: false
dependsOn: []
---

## Objetivo
Que cada tile muestre un icono acorde a su tipo (luz, cover, interruptor, relé, sensor…),
con una fuente de iconos bonita en vez de los símbolos genéricos de LVGL.

## Contexto
Ahora `item_icon()` en `ui-lvgl/main.c` devuelve símbolos LVGL (FontAwesome) genéricos. El TTF
de Material Design Icons está en `ui-lvgl/icons/mdi.ttf` y los codepoints elegidos son:
lightbulb F0335, window-shutter F111C, light-switch F097E, electric-switch F0E9F,
thermometer F050F, power F0425. Falta generar la fuente LVGL (el auto-mode bloqueó `npx`,
hay que correrlo a mano) e integrarla. Ver [[tpp06-lvgl-ui]].

## Criterios de aceptación
- [ ] `mdi28.c` generado con `lv_font_conv` (size 28, bpp 4) y añadido al `build.sh`.
- [ ] `LV_FONT_DECLARE(mdi28)` y la label del icono usa `mdi28` con el glifo UTF-8 del tipo.
- [ ] En el panel: luz→bombilla, cover→persiana, switch→interruptor, relé→relé/power, sensor→termómetro.
- [ ] El tile sigue coloreándose ON/OFF (ámbar/gris) con el icono visible.

## Notas técnicas
Comando: `npx -y lv_font_conv@1.5.3 --font icons/mdi.ttf --range 0xF0335,0xF111C,0xF097E,0xF0E9F,0xF050F,0xF0425 --size 28 --bpp 4 --format lvgl --no-compress -o mdi28.c`.
Codepoint→UTF-8 en runtime (4 bytes para U+F0xxx) o literales. Icono y color de icono conviven.

## Testing
Readback del framebuffer (BGR565→PNG) y foto del panel para confirmar los iconos por tipo.
