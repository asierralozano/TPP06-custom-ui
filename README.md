# TPP06 · UI estilo Tuya para Home Assistant

Interfaz personalizada para el panel **TPP06** (Tuya/MOES, SoC Anyka AK3760E,
Linux ARMv5, pantalla 320×480) que **recrea 1:1 la UI original de Tuya** pero
conectada a **Home Assistant**, no a la nube de Tuya.

## Estado

- **Clon de la UI de Tuya en C/LVGL** — 12 pantallas (home, dispositivos,
  detalles de luz/cover/termostato/media, sensor, escenas, salvapantallas,
  ajustes, centro de control, wizard de alta) + router de navegación, verificado
  en un **simulador de render en Mac**. Congelado en el tag **`c-lvgl-v1`**.
- **Migración a ESPHome (en curso, rama `esphome-migracion`)** — se re-expresa la
  UI en el `lvgl:` **nativo de ESPHome**, montada sobre la base de hardware del
  proyecto comunitario de Philiphall6 (ver créditos). Primera pantalla lista:
  `esphome/tpp06-home.yaml`.

## Estructura

| Carpeta | Qué hay |
|---|---|
| `ui-lvgl/` | Clon en C/LVGL (módulos `tuya_*`, router `ui_nav`) + simulador `sim/` |
| `esphome/` | Config ESPHome (UI nuestra + glue de hardware por referencia) |
| `agent/` | Agente MQTT en C (expone el panel a HA por MQTT discovery) |
| `docs/` | Spec de la UI, notas de integración, runbooks de instalación/recuperación |
| `scripts/` | Utilidades de arranque/diagnóstico del device |

## Créditos

- **Base de hardware ESPHome:** [Philiphall6/TPP06-smart-panel-project](https://github.com/Philiphall6/TPP06-smart-panel-project)
  (componentes `fbdev`/`evdev`/`host`/`tpp06_*`), usada **con permiso del autor**.
- [LVGL](https://lvgl.io) v8.3 · fuente [Manrope](https://github.com/sharanda/manrope)
  · [Material Design Icons](https://pictogrammers.com/library/mdi/).

## Seguridad / privacidad

**Nunca** se versiona: `secrets.yaml`, tokens de HA, credenciales MQTT, config WiFi,
contraseñas del panel ni volcados de NAND. Los binarios compilados llevan claves de
instalación: **compílalos en privado**. Los assets gráficos provienen del firmware
de Tuya (copyright de terceros) y **no se redistribuyen** en este repo.
