# Runbook: instalar la pantalla `home` en un panel de pruebas

Objetivo: compilar e instalar **solo la pantalla `home`** (`esphome/tpp06-home.yaml`)
en un TPP06 de pruebas, verla en el hardware real y, si clava, iterar por OTA.
Primera instalación en panel stock = intervención avanzada (reversible a Tuya).

> Fallback = **Tuya de fábrica**, no rollback automático. Ten backups y
> recuperación on-site (UART, ver `docs/RECUPERACION-UART.md`) antes de empezar.

## Requisitos

- Panel **compatible**: variante `ak3761e_tpp06z`, kernel `4.4.192V2.4`.
  *Si no puedes confirmarlo, PARA antes de escribir nada en el panel.*
- Home Assistant con **ESPHome Device Builder**, versión probada **2026.10.2**.
- PC en la misma LAN con `python3`, `git`, `adb` (Android Platform Tools).
- El repo base clonado (trae las herramientas):
  `git clone https://github.com/Philiphall6/TPP06-smart-panel-project.git`
- IP del panel (de la lista de dispositivos del router), alimentación estable.

## 0. Confirmar la variante (gate de compatibilidad)

Desde el repo base, lee la [guía START-HERE](https://github.com/Philiphall6/TPP06-smart-panel-project/blob/main/docs/en/START-HERE.md)
y confirma la variante. El aspecto no garantiza compatibilidad.

## 1. Acceso de red + AUTHKEY

```bash
cd TPP06-smart-panel-project
python3 tools/tpp06_authkey_reader.py <IP_PANEL>
```
Devuelve el AUTHKEY (enmascarado) y deriva la pass root. **No** escribe ni
reinicia. Guarda la salida en privado. (Ya teníamos acceso propio por Dropbear;
esta herramienta es la vía del flujo base.)

## 2. Backup — gate de seguridad (NO saltar)

```bash
python3 tools/tpp06_tuya_cutter.py <IP_PANEL> status
python3 tools/tpp06_tuya_cutter.py <IP_PANEL> backup
python3 tools/tpp06_tuya_cutter.py <IP_PANEL> network-check
```
Espera la verificación del backup y guárdalo en almacenamiento aparte.
(Cubre el alcance del Cutter, no toda la NAND; nosotros además tenemos
`nand-backup/` del panel original.)

## 3. Cargar NUESTRA config en Device Builder

1. Copia `esphome/tpp06-home.yaml` de ESTE repo a `/config/esphome/` de HA
   (p. ej. `panel-pruebas.yaml`) con el editor de ficheros de HA. Aparecerá en Builder.
   **No** uses el asistente de crear ESP32.
2. **Edit** → ajusta las `substitutions` de arriba:
   | Variable | Tu valor |
   |---|---|
   | `device_name` | nombre técnico único sin espacios, p. ej. `panel-pruebas` |
   | `weather_entity` | tu entidad `weather.…` de HA (Dev tools → States) |
3. Crea/edita el `secrets.yaml` de Builder con una clave nueva:
   ```bash
   openssl rand -base64 32
   ```
   ```yaml
   tpp06_api_encryption_key: "PEGA_AQUI_LA_CLAVE"
   ```
   (Plantilla en `esphome/secrets.yaml.example`. Una clave por panel.)
4. **No toques** `api`, `ota`, `host`, `external_components` ni
   `api.reboot_timeout: 0s`. No añadas `wifi:` de ESP32 (el supervisor conserva
   el WiFi de Linux). Nuestra config ya fija `external_components` al tag `v0.1.7`.

## 4. Validar y compilar

En Builder: **Validate** → **compile / manual download** (sin instalar).
El resultado es un ejecutable Linux llamado `program` (no un `.bin` de ESP32).
Usa ESPHome **2026.10.2** para que cuadre con los drivers `v0.1.7`.

## 5. Primera instalación (panel stock) + prueba guardada

Sigue el flujo del repo base (Tuya Cutter / procedimiento asistido) para
desplegar `program` y arrancarlo bajo su supervisor. Es la activación inicial
reversible. Verifica en el panel: wallpaper de Tuya, reloj Manrope 90, fecha y
temperatura del `weather_entity`. Comprueba en los logs de HA que la **API
conecta y autentica** ("online" no basta).

## 6. Iterar por OTA

Con ESPHome ya corriendo, cada cambio de la `home` se envía **por OTA** desde
Builder (mantén la misma clave API). Aquí es donde afinamos al píxel colores y
tamaños contra la pantalla real.

## Seguridad

- `secrets.yaml` y el ejecutable `program` (llevan claves) **nunca** a git.
- No reiniciar a ciegas; backups verificados y recuperación UART a mano.
- Assets de Tuya (`tuya-fw/`) no se redistribuyen.
