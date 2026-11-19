# TPP06 — Orquestador de tareas

UI nativa (LVGL) + persistencia + producto para el panel TPP06 (Anyka AK3760E).
El loop de desarrollo coge la siguiente tarea `todo` con dependencias `done`, la implementa,
la prueba en el panel y la marca `done`.

**Total de tareas: 11** (10 activas + 1 opcional)

## Estados
- ⬜ `todo` — pendiente, lista para cogerse cuando sus dependencias estén `done`
- 🔵 `in_progress` — en curso
- ⛔ `blocked` — bloqueada por algo externo
- ✅ `done` — terminada y validada
- ⭐ `optional` — opcional / nice-to-have (no bloquea nada)

## Épicas
- **E1 · UI (LVGL)** — interfaz, editor, iconos, widgets, salvapantallas
- **E2 · Config & teclado** — WiFi, MQTT y Home Assistant editables desde el panel
- **E3 · Persistencia/Sistema** — modo reemplazo, supervisor, arranque
- **E4 · Producto/Instalador** — instalador plug-and-play para los 4 paneles
- **E5 · Integración HA** — MQTT en modo reemplazo, detalles por tipo de entidad

## Tabla maestra

| ID | Épica | Título | Estado | Est. | Depende de |
|----|-------|--------|--------|------|------------|
| E1-T01 | E1 · UI | Iconos MDI por tipo de entidad en los tiles | ⬜ todo | M | — |
| E1-T02 | E1 · UI | Nombres editables e iconos personalizables por tile | ⬜ todo | M | — |
| E1-T03 | E1 · UI | Widgets propios no-entidad (clima/tiempo, …) | ⬜ todo | M | — |
| E1-T04 | E1 · UI | Mejorar/clonar el salvapantallas del firmware | ⭐ opcional | S | — |
| E2-T01 | E2 · Config | WiFi desde la UI: escanear + conectar (wpa) con teclado | ⬜ todo | L | — |
| E2-T02 | E2 · Config | Editar MQTT del agente (tpp06.conf) con teclado | ⬜ todo | M | — |
| E2-T03 | E2 · Config | Introducción del token de HA más fácil (QR/pairing/paste) | ⬜ todo | M | — |
| E3-T01 | E3 · Sistema | Validar modo reemplazo con reinicio real | ⛔ blocked (UART) | S | E2-T01 |
| E4-T01 | E4 · Instalador | Instalador plug-and-play USB para los 4 paneles | ⬜ todo | L | E3-T01 |
| E5-T01 | E5 · HA | Exponer el panel a HA por MQTT en modo reemplazo | ⬜ todo | M | E3-T01 |
| E5-T02 | E5 · HA | Detalles (long-press) por tipo de entidad + arreglar covers | ⬜ todo | L | — |

## Hitos
- **M1 · Producto base en el panel**: E3-T01, E2-T01, E2-T03, E4-T01 (instalador funcionando en los 4 paneles).
- **M2 · UI rica completa**: E1-T01, E1-T02, E1-T03, E2-T02, E5-T02.
- **M3 · Integración HA avanzada**: E5-T01, E5-T02.
- **Opcionales**: E1-T04.
