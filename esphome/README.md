# Migración a ESPHome — base Philiphall6 + UI nuestra

Rama `esphome-migracion`. Partida: el clon de la UI de Tuya existe y está verificado
en C/LVGL (tag `c-lvgl-v1` en `master`). Aquí lo re-expresamos en el `lvgl:` **nativo
de ESPHome**, montado sobre la **base de hardware del repo comunitario Philiphall6**
(con su **permiso explícito** para usar su repo y todos sus avances).

> Nota de prudencia: para un producto conviene tener ese permiso **por escrito**
> (que añadan un `LICENSE`, o guardar su mensaje). No bloquea nada; protege a futuro.

## Por qué esta vía

Nuestro activo no es el código C, es el **conocimiento del layout exacto de Tuya**
(coordenadas, colores, fuentes, assets — en `docs/UI-TUYA-SPEC.md` y los `tuya_*`).
Eso es justo la entrada del YAML de ESPHome → migrar = **transcribir**, no rediseñar.
Y así heredamos su OTA, su cross-compile para el Anyka, y la API nativa de HA.

## Archivo principal: `tpp06-home.yaml`

Config **completa y compilable** (no un stub): glue de Philiphall6 + nuestra `home`.

- **Glue por referencia (no vendorizado):** `external_components: type: git` apunta a
  su repo, `path: esphome-components`, fijado al tag **`v0.1.7`**. Reusa `fbdev`
  (display `/dev/fb0` RGB565 320×480), `evdev` (táctil `/dev/input/event1`), `host`
  (cross-compile Anyka), `tpp06_compat`, `tpp06_mdns`. Su código vive en su repo; lo
  consumimos como dependencia pinada. Para producción, mantener el `ref:` en un tag.
- **UI nuestra:** `page_home` 1:1 de `ui-lvgl/tuya_home.c` — wallpaper + icono clima
  reales, Manrope 16/19/90, hora/fecha/temperatura atadas a HA.
- **Win de fuentes:** `gfonts://Manrope@500` baja Manrope con el **peso exacto** →
  resuelve el problema de peso que teníamos con la variable-font en C (rendía ~400).

## Mapa de equivalencias (C → ESPHome lvgl) — validado contra su config

| Nuestro (C) | ESPHome `lvgl:` |
|---|---|
| `lv_obj` + estilos | `obj` con `widgets:` anidados |
| `lv_label` + `manrope(n)` | `label` + `font:` horneada |
| `lv_img` PNG | `image:` / widget `image` (`src`) |
| slider / bar | `slider` (`on_release` + `data_template`) |
| router `ui_nav` (pila) | `pages` + `lvgl.page.show` |
| `ha_call(...)` | `homeassistant.action` |
| `ha_get` / estados | `homeassistant` sensor/text_sensor + `lvgl.label.update` |

## Requisitos para compilar (gated por el panel)

1. `secrets.yaml` del device con `tpp06_api_encryption_key` (nunca en git).
2. Assets en `../tuya-fw/img/...` (presentes en local; copyright Tuya, gitignored).
3. Toolchain ESPHome (su base selecciona compilador: Zig en ARM64 / Bootlin en x86-64).
4. `esphome compile esphome/tpp06-home.yaml` y, con el panel en WiFi, OTA.

## Cómo seguir

1. (Gated) Compilar `tpp06-home.yaml` y ver `home` en el panel. Validar fuentes,
   assets y binding de hora/clima.
2. Si clava → transcribir las 11 pantallas restantes (mecánico, con el spec en mano):
   dispositivos, detalles luz/cover/termostato/media, sensor, escenas, salvapantallas,
   ajustes, centro de control (usa `tpp06_backlight` para el brillo), wizard añadir.
3. Reusar más glue según haga falta: `tpp06_backlight` (brillo), `tpp06_local_relays`
   (relés físicos), `tpp06_wifi` (onboarding WiFi del instalador).
