# Especificación de la UI de Tuya (TPP06) — para recreación 1:1 en nuestro LVGL

> Fuente: firmware OTA **1.2.3** (`~/Downloads/tyznzk_TPP06_AK_OTA_1.2.3.img`), extraído a
> `~/Documents/TPP06-custom-ui/tuya-fw/`. La UI de Tuya es **LVGL + Lua** (`pages.json`:
> `"upgrade_type":"LINUX_LVGL"`, framework 2.3). **El Lua es código fuente legible** → specs exactos.
> Esto es la base para clonar la UI pantalla a pantalla con entidades de Home Assistant.

## 0. Provenance / dónde está cada cosa
```
tuya-fw/
  lua_script/
    pages.json                 # registro de páginas de detalle por tipo
    init_script/project_init.lua
    theme/desktop.lua          # HOME: 5 estilos de reloj/escritorio (specs exactos)
    theme/screensaver.lua      # salvapantallas
    common_script/category_ui_base.lua   # paleta + helpers (create_label/obj/page)
    gui_cont_page/             # TARJETAS del home por tipo (cont_light, cont_curtain, cont_temp…)
    gui_light/ gui_curtain/ …  # PÁGINAS DE DETALLE por tipo
    mod_lua/time.lua weather.lua theme_manage.lua theme_widget.lua
    common/{base,engine,lib,loader,service}   # engine LVGL-sobre-Lua
  img/     # iconos/fondos PNG (icon/, theme/, weather/, screen_saver/, card/, center/…)
  font/Manrope-VariableFont_wght.ttf   # FUENTE OFICIAL de la UI (Latina)
  gif/     # animaciones
  pkg_res/ # recursos empaquetados
```

## 1. Globales
- **Pantalla:** 320×480 (`category_ui_base.w/h`). Header de detalle: `h_head = 70`.
- **Paleta base:** `white=0xFFFFFF`, `black=0x000000`. Acentos conocidos:
  - Verde ON (switch luz): **`0x57C891`**
  - Naranja offline: **`0xEFA34F`**
  - (Los `TOPIC_*` del tema se resuelven por índice en runtime; capturar hex exactos de
    `pkg_res`/theme o de screenshot. Topics clave: `NEXT_SWITCH_BG=102`, `NEXT_SCREEN_SLIDER=73`,
    `NEXT_SCREEN_SLIDER_BG=74`, `DEV_HOME_BG=16`, `SCENE_BG=32`, `SHORT_CUT_BG=8`.)
- **Fuente:** **Manrope** (variable). `gui_vfont_get(size, weight)` = Manrope por defecto;
  `gui_custom_vfont_get("Manrope-VariableFont_wght.ttf", size, weight)` para el reloj grande.
  Pesos usados: 400/500/600/700. → en LVGL: generar Manrope en los tamaños/pesos necesarios.
- **Escalado:** `e.dpx(n)` = n escalado por densidad (en 320px ≈ 1:1). Tratar dpx≈px.

### Mapeo engine Lua → LVGL C (directo)
| Lua (`e.`/`ui_base.`) | LVGL C |
|---|---|
| `obj_create`, `create_obj(p,w,h,r,bg,opa)` | `lv_obj_create` + size/radius/bg |
| `label_create`/`create_label(p,x,y,txt,sz,wt,align,color,opa)` | `lv_label` + Manrope font |
| `img_create`/`img_set_src(utils.img_path("…"))` | `lv_img` + asset PNG |
| `bar_create`/`bar_set_value/range` | `lv_bar` |
| `obj_align(o,ALIGN_*,x,y)` | `lv_obj_align` |
| `color_hex(0x..)` | `lv_color_hex` |
| `img_set_src("icon/on.png")`, recolor | `lv_img_set_src` + recolor |

## 2. HOME — pantalla 0: reloj/escritorio (`theme/desktop.lua`)
Widget de reloj a pantalla completa, **320×310** (de 480). Hay **5 estilos** (el usuario elige):

- **Estilo 0 (default):** icono clima 36px arriba-izda (cont 74×64 @5,15); temp `26℃` 19/500;
  fecha `06月24日 星期一` 16/500 @(30,66); **hora `06:30` a 105px** (90 si 24h) Manrope 500,
  **centrada**, ancho=pantalla; AM/PM (si 12h) 22px @(265,171).
- **Estilo 1:** hora **vertical** `06\n30` 105px @(18,8), line-space −40; clima 68px @(188,30);
  temp/fecha a la derecha.
- **Estilo 2:** hora `06:30` 86/500 @(17,25); clima 24px @(183,128); fecha abajo-izda.
- **Estilo 3:** fecha grande centrada 34/600 @top; hora 86/700 @(19,73); clima 68px.
- **Estilo 4:** clima 80px centrado arriba; hora 68/700 centrada @y120; fecha 24/600.
- Refresco cada 5 s (timer). Clima vía `weather.lua`; iconos en `img/weather/<size>/<code>.png`.

→ **Nuestro home pantalla 0** = uno de estos estilos (empezar por el 0) con reloj Manrope + (opcional)
un widget de clima HA; debajo, swipe horizontal → pantalla(s) de tarjetas.

## 3. HOME — tarjetas de dispositivo (`gui_cont_page/*`, "next screen")
Grid de **tarjetas** con controles en línea. Dos anchos: **`NS_1_W`** (media, para sensores/temp/
IPC/switch-simple) y **`NS_2_W`** (ancha completa, para luz/cover/dimmer/aroma/música). Alto común
`NS_H`. (Valores px exactos: pendientes de `engine.base`/screenshot.)

**Anatomía tarjeta de LUZ (`cont_light.lua`) — ejemplo canónico:**
- Botón switch **circular** 56×56 (radius=56) @(20,103). ON → fondo verde `0x57C891` + `icon/on.png`
  opa100; OFF → fondo tema `TOPIC_NEXT_SWITCH_BG` + `icon/off.png` opa60.
- **Barra de brillo** (slider horizontal) ancho=`NS_2_W−98−24`, alto 90 @(98,86), radius 16; fondo
  `TOPIC_NEXT_SCREEN_SLIDER_BG`, indicador `TOPIC_NEXT_SCREEN_SLIDER`. Label `NN%` Manrope 20/600
  negro opa70 dentro, left-mid @(19,0). Arrastre horizontal cambia brillo (gesto con histéresis).
- Offline → label naranja `0xEFA34F` 18/400 @(22,55).
- DP Tuya: `switch_led`, `work_mode`, `home_ctrl_light_bright`, `home_ctrl_light_colour`.

Otras tarjetas: `cont_curtain` (persiana, controles abrir/parar/cerrar + posición), `cont_switch`
(1..N gangs), `cont_temp`/`cont_sensor_common`/`cont_wsdcg` (sensores, media anchura),
`cont_scene`/`cont_sc_scene` (escenas), `cont_dimmer_switch`, `cont_aroma`, `cont_water_heater`,
`cont_music`/`cont_qq_music`, `cont_ipc`. Base común: `cont_page_base.lua`.

## 4. Página de DETALLE (patrón común, `category_ui_base.page_create`)
Pantalla completa 320×480 con:
- **Header 320×70** negro: icono back `icon/left.png` recolor blanco @(24,21); título Manrope 20/600
  @(58,20) (texto acortado a ~9 chars); **mitad izda. clickable** = volver; **swipe** = volver
  (anim `TY_PAGE_ANIM_TP_OUT_SIMPLE`).
- **Contenido** 320×480 (header en foreground), opción de imagen de fondo.
- Páginas registradas en `pages.json` (id→lua): `light, dimmer_switch, switch, scene_switch,
  curtain, airer, air_conditioner, thermoregulator, humidifier, water_heater, socket_one,
  aroma_diffuser, air_purifier, fresh_air, sweeper`, + familia infrarrojos (`ir_*`, `infrared_*`).

## 5. Salvapantallas / inactividad
`theme/screensaver.lua` + `img/screen_saver/` (incl. `carousel_mask.png` 320×108). Clonar reloj en
salvapantallas + comportamiento de inactividad (atenuar → apagar backlight → despertar al tocar).

## 6. Mapeo a Home Assistant (lo nuestro)
Las tarjetas/detalles de Tuya leen **DPs**; nosotros leemos **entidades HA** (dominio/servicio):
| Tuya | HA |
|---|---|
| luz: switch_led/bright/colour | `light` on/off, `brightness_pct`, `rgb/hs` |
| curtain abrir/parar/cerrar/pos | `cover` open/stop/close/`set_cover_position` |
| switch (gangs) | `switch`/`light` |
| sensor temp/hum | `sensor` (solo lectura) |
| scene | `scene`/`script` activar |
Añadir entidades = nuestro wizard (2×2 tipos → lista) ya existente, reusando estas tarjetas.

## 7. Plan de implementación (fases)
1. **Assets**: convertir Manrope a fuentes LVGL (tamaños 105/86/68/34/24/20/18/16, pesos 400–700);
   copiar PNGs necesarios (`icon/on,off,left`, weather, screensaver) a assets LVGL (C arrays o LVGL fs).
2. **Home P0**: reloj estilo 0 (Manrope 105 + clima + fecha), fondo.
3. **Tarjetas**: `cont_light` (switch circular + slider brillo), `cont_curtain`, `cont_switch`,
   `cont_sensor/temp`, `cont_scene` — cada una cableada a HA; grid NS_1/NS_2.
4. **Detalle**: header común + detalle por tipo (empezar luz y cover).
5. **Salvapantallas + inactividad** clonados.
6. **Ajustes** (WiFi/fondo/MQTT/HA) ya iniciados; alinear estética.
7. Integrar con el **wizard de añadir** entidades HA.

## Pendiente de extraer (siguiente pasada)
- `engine.base`: valores px de `GUI_DESKTOP_NS_1_W/NS_2_W/NS_H`, `GUI_SCREEN_*`, `dpx`.
- Hex reales de los `TOPIC_*` del tema (de `pkg_res`/theme o screenshot).
- `gui_curtain/curtain.lua`, `gui_switch/switch.lua`, settings/`center` screens, `screensaver.lua`.
