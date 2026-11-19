# Migración a ESPHome — spike de UI

Rama `esphome-migracion`. Punto de partida: el clon de la UI de Tuya ya existe y
está verificado en C/LVGL (tag `c-lvgl-v1` en `master`). Aquí lo re-expresamos en
el `lvgl:` **nativo de ESPHome** para ganar: OTA, API de HA nativa, páginas/eventos
y cero pegamento en C a largo plazo.

## Por qué esta vía

Nuestro activo valioso **no es el código C**, es el **conocimiento del layout exacto
de Tuya** (coordenadas, colores, fuentes, qué asset va en cada sitio — en
`docs/UI-TUYA-SPEC.md` y los módulos `tuya_*`). Ese conocimiento es justo la entrada
para escribir el YAML de ESPHome. Migrar = **transcribir**, no rediseñar.

## Qué prueba `tuya_home.spike.yaml`

1. **Fuentes Manrope** horneadas al tamaño exacto (16/19/90) vía `gfonts://Manrope@<peso>`.
   → Bonus: ESPHome baja la fuente con el **peso exacto** (500/700/800), lo que
   **resuelve el problema de peso** que teníamos con la variable-font en C (rendía ~400).
2. **Assets reales de Tuya** (`theme/bg/00.png`, `weather/36/sunny.png`) como `image:`.
3. **Binding a HA**: hora (local), fecha y temperatura del `weather.home`.

Esto cubre la **capa de UI**, que es la parte nuestra y portable. No incluye el
arranque de hardware (ver abajo).

## Mapa de equivalencias (C → ESPHome lvgl)

| Nuestro (C) | ESPHome `lvgl:` |
|---|---|
| `lv_obj` + estilos (tarjeta) | `obj` con estilos |
| `lv_label` + `manrope(n)` | `label` + `font:` horneada |
| `lv_img` PNG | `image:` (horneado en build) |
| slider / bar | `slider` / `bar` |
| router `ui_nav` (pila) | `pages` + acción `lvgl.page.show` |
| flecha volver | `image` + `on_click` → `page.show` |
| `ha_ws` (WebSocket) | `homeassistant` sensors + `homeassistant.action` |

## Pendiente / decisiones abiertas

- **Glue de hardware (display/táctil/relés/backlight).** El spike deja comentado el
  `display:` y la plataforma `host:`. Dos caminos, porque el repo de Philiphall6
  **no tiene licencia** (= derechos reservados, no podemos copiar sus componentes):
  1. **Pedirles licencia** (issue amable; proyecto comunitario → probable MIT/Apache).
  2. **Reimplementar** el glue nosotros desde la doc pública de ESPHome + lo que ya
     sabemos del hardware (ABI de display/ioctls, relés GPIO, táctil `event0` —
     todo en nuestras memorias). Esquiva la licencia por completo.
- **Fuentes**: enumerar los ~10 tamaños Manrope que usan las 12 pantallas y su peso.
- **Assets**: `tuya-fw/img` es copyright Tuya (gitignored). En el build real se
  copian a `esphome/assets/` o se referencia la ruta; no se redistribuyen.
- **Simulador**: se pierde el loop de render en Mac; el preview pasa a ser compilar
  con ESPHome (plataforma `host:` corre nativo) o verlo en el panel.

## Cómo seguir

1. (Gated por el panel) Compilar este spike con el glue mínimo y mirar `home`.
2. Si clava → transcribir las 11 pantallas restantes (mecánico, con el spec en mano).
3. En paralelo: decidir camino de licencia (1 o 2) para el glue de hardware.
