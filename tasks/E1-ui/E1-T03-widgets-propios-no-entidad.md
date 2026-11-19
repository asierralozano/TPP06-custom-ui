---
id: E1-T03
epic: E1 · UI
title: Widgets propios no-entidad (clima/tiempo, …)
status: todo
estimate: M
optional: false
dependsOn: []
---

## Objetivo
Soportar en el catálogo widgets que no son entidades de HA ni relés: empezar por un widget de
**tiempo/clima** y dejar la arquitectura para añadir más (reloj grande ya existe en la página 0).

## Contexto
El wizard de "Añadir" hoy ofrece Relés / Luces / Covers / Interruptores (entidades). El usuario
quiere también widgets generados por nosotros. El clima puede venir de una entidad `weather.*` de
HA (vía plantilla) o de una API. Ver [[tpp06-ui-editor-vision]].

## Criterios de aceptación
- [ ] Nueva categoría "Widgets" en el wizard con al menos "Tiempo/Clima".
- [ ] El tile de clima muestra temperatura y estado (icono) y se actualiza.
- [ ] Persiste en el layout como un tipo de item propio (p.ej. `widget=weather.casa|Tiempo|weather|page`).
- [ ] Arquitectura preparada para añadir más tipos de widget propios.

## Notas técnicas
Si se usa `weather.*` de HA, reutilizar `subscribe_entities`/plantillas; si es API externa, HTTP
desde el binario (como `http_post_template`). Icono de clima vía la fuente MDI (E1-T01).

## Testing
Prueba física: añadir el widget de clima y ver la temperatura real; readback para el render.
