---
id: E4-T01
epic: E4 · Instalador
title: Instalador plug-and-play USB para los 4 paneles
status: todo
estimate: L
optional: false
dependsOn: [E3-T01]
---

## Objetivo
Un instalador que, al enchufar un panel por USB y ejecutarlo, lo deje todo funcionando:
AUTH, backup NAND, push de dropbear + UI + config, y persistencia. "Clic y funcionar".

## Contexto
El usuario tiene 4 paneles (1 de pruebas + 3 en casa). El de pruebas ya está montado a mano; hay
que formalizarlo en un script repetible y config-driven. Existen piezas: `tpp06_exec.py` (root por
USB), `tpp06_usb_probe`/`tpp06_usb_shell`, backup NAND, boot.sh replace. Ver [[tpp06-acceso-scriptado]],
[[tpp06-vision-producto]], [[tpp06-ssh-persistente]].

## Criterios de aceptación
- [ ] Un comando (p.ej. `./instalar.sh`) detecta el panel por USB/ADB y hace: AUTH → backup NAND →
      push de `dropbear` + `tpp06_lvgl` + config + claves SSH → instala el boot.sh de reemplazo.
- [ ] Idempotente y con logs claros; si algo falla, no deja el panel inservible (recuperación).
- [ ] Config por panel (device-id de la MAC ya se autodetecta); parámetros (HA, WiFi) inyectables.
- [ ] Probado en el panel de pruebas de principio a fin; listo para los otros 3.

## Notas técnicas
Reutilizar las herramientas existentes. La contraseña/clave por env, nunca en git. Depende de que
el modo reemplazo esté validado (E3-T01). Ideal: integrar la config de WiFi (E2-T01) y token HA
(E2-T03) en el flujo del instalador.

## Testing
Ejecutar el instalador sobre el panel de pruebas desde cero (restaurando antes el estado stock).
