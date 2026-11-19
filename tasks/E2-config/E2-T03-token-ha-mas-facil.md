---
id: E2-T03
epic: E2 · Config
title: Introducción del token de HA más fácil (QR/pairing/paste)
status: todo
estimate: M
optional: false
dependsOn: []
---

## Objetivo
Hacer fácil introducir la configuración de Home Assistant (sobre todo el **token**, que es muy
largo) sin tener que teclearlo entero en el panel.

## Contexto
Ya existe la edición de HA (host/puerto/token) con teclado en pantalla, pero teclear un token de
larga duración (~180 chars) a mano es inviable en producción. Hay que mantener la opción manual
pero idear algo mejor. Ver [[tpp06-lvgl-ui]], [[tpp06-vision-producto]].

## Criterios de aceptación
- [ ] Sigue existiendo la edición manual (host/puerto/token con teclado).
- [ ] Al menos un método "fácil" implementado o prototipado, p.ej.:
  - QR mostrado por el panel que lleva a una web/app donde pegar el token y éste llega al panel; o
  - flujo de emparejamiento (el panel se registra en HA y recibe credenciales); o
  - recibir el token por MQTT/HTTP local desde el instalador (E4-T01).
- [ ] El token se guarda cifrado/solo en el panel, nunca en git.

## Notas técnicas
Opciones a valorar: `webhook` de HA + QR; `long-lived token` generado por el instalador y enviado
por la red local; login OAuth de HA (complejo). Decidir la más simple para el instalador.

## Testing
Configurar un panel nuevo sin teclear el token a mano y comprobar que conecta a HA.
