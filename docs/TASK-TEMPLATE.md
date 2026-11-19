---
id: E<épica>-T<NN>
epic: E<n> · <nombre épica>
title: <título corto>
status: todo            # todo | in_progress | blocked | done
estimate: S            # S | M | L
optional: false        # true si es "nice to have"
dependsOn: []          # lista de IDs (p.ej. [E3-T01])
---

## Objetivo
Qué hay que conseguir, en una o dos frases.

## Contexto
Por qué, de dónde viene, y en qué parte del código/sistema toca.
Referencias a memorias del proyecto o ficheros (`ui-lvgl/main.c`, `boot.sh`, …).

## Criterios de aceptación
- [ ] Criterio verificable 1
- [ ] Criterio verificable 2

## Notas técnicas
Pistas de implementación, APIs, gotchas conocidos.

## Testing
Cómo se valida (readback del framebuffer, prueba física en el panel, logs…).
