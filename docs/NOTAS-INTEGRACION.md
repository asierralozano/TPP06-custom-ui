# Integración del clon Tuya en el device (andamiaje → HA)

Estado: **andamiaje de navegación listo y verificado en el simulador** (`ui-lvgl/sim/sim_nav.c`).
Falta el paso con hardware (gated por recuperar el panel por UART). Este doc deja escrito el
plan de enganche para hacerlo del tirón cuando el panel vuelva.

## Piezas ya construidas (todas en `ui-lvgl/`, compartidas device+sim)

- **Pantallas** (`tuya_*`): `tuya_home`, `tuya_cards` (tarjetas: luz/interruptor + `tuya_card_sensor` +
  `tuya_devices_build` / `tuya_devices_mix_build`), `tuya_detail` (luz/cover/termostato + `page_bg_header`),
  `tuya_media` (card + detalle), `tuya_scenes`, `tuya_settings`, `tuya_control_center`,
  `tuya_add_entity`, `tuya_screensaver`.
- **Router** (`ui_nav.c/.h`): pila de pantallas, `ui_nav_init/go/push/back/current`, transiciones
  LVGL (`lv_scr_load_anim`, auto_del). Los detalles leen `g_nav_ctx` (name/on/bright/position/
  setpoint/current/volume/playing + line1/line2).
- **Hook de volver** (`tuya_ui.c/.h`): `tuya_ui_set_back_cb()` + `tuya_ui_attach_back()`; todas las
  cabeceras enganchan su flecha. El router fija el cb a `ui_nav_back`.
- **Verificado**: `sim/sim_nav.c` ejercita HOME→go DEVICES→push DETALLE(contexto)→back→CONTROL y
  confirma que la pila y el contexto funcionan (ver `sim/nav_*.png`).

## Lo que hace el `main.c` actual (a conservar)

- `ha` (cliente `ha_ws`), `ha_watch/connect/poll/call/call_bri/call_cover_pos/get`.
- Modelo `Item[]` (tiles del home) con config persistida (`load_config`/`save_config`,
  `/tuya/data/custom/tpp06_ui.conf`), editor drag/rename/borrar, relés y botones físicos.
- `build_home`/`rebuild_home`/`open_detail` (UI propia actual) — se irá sustituyendo por los `tuya_*`.

## Plan de enganche (cuando vuelva el panel)

### 1. Build
Añadir a `ui-lvgl/build.sh` (línea del cross-compiler) los módulos nuevos:
`tuya_home.c tuya_cards.c tuya_detail.c tuya_media.c tuya_scenes.c tuya_settings.c
tuya_control_center.c tuya_add_entity.c tuya_screensaver.c tuya_ui.c ui_nav.c assets.c`
(ya estaba `fonts.c`). Confirmar que compila `-static -Os -march=armv5te -mfloat-abi=soft -marm`.

### 2. Arranque de UI en `main()`
Sustituir la llamada a `build_home()` por `ui_nav_init()` tras registrar display+indev.
El reloj (`clock_update` timer) y el `ha_poll` del bucle principal se mantienen.

### 3. Datos de HA → pantallas
- **Home/Dispositivos**: `tuya_cards`/`tuya_devices_build` deben pasar a **iterar `Item[]`** (no demo):
  por cada item, `ha_entity *e = ha_get(&ha, item->entity)` y rellenar nombre/estado/brillo.
  Opción limpia: añadir `tuya_card_from_item(parent,x,y,w,Item*)` que lee `e->state`/`e->brightness`.
- **Detalle**: antes de `ui_nav_push(SCR_DETAIL_*)`, rellenar `g_nav_ctx` desde la `ha_entity`:
  - luz: `name`, `on = state_is_on(e->state)`, `bright = e->brightness*100/255`.
  - cover: `position = e->position`.
  - termostato/media: mapear los DP/atributos equivalentes (requiere vigilar esos attrs en `ha_ws`).
- **Refresco en vivo**: cuando `ha_poll()` devuelva 1, si la pantalla actual (`ui_nav_current()`)
  muestra entidades, refrescar sus labels/sliders (o reconstruir la pantalla actual). Para detalles,
  guardar punteros a los widgets (como hace `open_detail` hoy) o exponer `tuya_detail_light_update()`.

### 4. Controles → HA (callbacks)
Los `tuya_*` hoy son solo visuales. Añadir eventos (como en el `main.c` actual):
- tarjeta: `LV_EVENT_SHORT_CLICKED` → `ha_call(&ha,"homeassistant","toggle",entity)`;
  `LV_EVENT_LONG_PRESSED` → set `g_nav_ctx` + `ui_nav_push(SCR_DETAIL_*)`.
- detalle luz: switch → `ha_call(... light toggle)`; slider brillo → `ha_call_bri`.
- detalle cover: abrir/parar/cerrar → `ha_call(cover, open/stop/close_cover)`; (pos → `ha_call_cover_pos`).
- termostato ±: `ha_call` a `climate.set_temperature` (añadir helper en `ha_ws`).
- media: play/pause/prev/next/vol → `media_player.*` (añadir helpers).
- escenas: tile → `ha_call("scene","turn_on",scene_id)` o `script.turn_on`.

### 5. Navegación global (gestos/botones)
- Deslizar desde arriba → `ui_nav_go(SCR_CONTROL)` (centro de control).
- Botón físico / icono ⚙️ del control → `ui_nav_go(SCR_SETTINGS)`.
- ✏️ Editar → modo editor actual (drag/rename/borrar) sobre la home.
- "+" del control o de dispositivos → `ui_nav_push(SCR_ADD_ENTITY)`.
- Inactividad (timer) → `ui_nav_go(SCR_SCREENSAVER)`; toque → `ui_nav_back()`/HOME.
- El wizard `tuya_add_entity` debe reusar la lógica de alta que ya existe (añadir `Item` + `save_config`).

### 6. Afinado con el panel
- Verificar colores de tema que dibuja el motor C de Tuya con un **screenshot real** (los `TOPIC_*`).
- Ajustar dimensiones de tarjeta y paddings al píxel contra la captura.
- Confirmar que la fuente grande (reloj 90, salvapantallas 150) rinde bien en el hardware (el fix de
  `lv_lru` ya está en `fonts.c`).

## Seguridad / límites
- Token HA solo en `/tuya/data/custom/tpp06_ui.conf` y scratchpad; nunca en git/memoria.
- No tocar la NAND; no reiniciar a ciegas (el panel ya quedó varado una vez). Recuperar primero por UART.
