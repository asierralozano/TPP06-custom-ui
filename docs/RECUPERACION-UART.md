# Recuperación del panel TPP06 por UART

> Estado: el panel quedó inaccesible tras reiniciar en **modo reemplazo** (sin Tuya →
> sin WiFi ni ADB; se queda en el logo MOES). No está bricado. Esta guía lo devuelve a
> un arranque con red (Tuya) para volver a trabajar.

## Hardware
- Adaptador **USB‑UART a 3.3 V** (p.ej. Waveshare FT232RNL, **switch en 3.3 V**).
- **UBICACIÓN DE PADS (confirmada por foto, placa TPP06-Z-AK V1.0.2, cara trasera):**
  en el **centro** de la placa, serigrafía vertical `VDD_CPU → SPI_DO → CLK → GND → RX0 → TX0`.
  `RX0` y `TX0` son un par de pads (uno sobre otro); el pad está a la **izquierda** de cada texto.
  `GND` utilizable justo encima (grupo `CLK/GND`). Son pads SMD pequeños → soldar 3 hilos finos.
- Conexión (cruzado):
  - adaptador **GND** ↔ panel **GND**
  - adaptador **RX** ↔ panel **TX0**
  - adaptador **TX** ↔ panel **RX0**
  - **NO** conectar VCC (el panel tiene su alimentación).
- Otros pads rotulados visibles (referencia): `KC_CTL1/2/3` (relés), `RXD_WIFI/TXD_WIFI` (UART del
  módulo WiFi, NO es la consola), `VBUS/DP1/DN1` (USB-C), `ZIG_*` (Zigbee).

## Terminal en el Mac
```sh
ls /dev/cu.usbserial-*
screen /dev/cu.usbserial-XXXX 115200   # 8N1
# (salir de screen: Ctrl-A, luego K, luego Y)
```

## Datos confirmados del arranque (del backup NAND, mtd1/mtd2)
- Consola: `ttySAK0,115200n8`.
- `bootdelay=1`, mensaje `Hit any key to stop autoboot`, prompt `anyka$` **sin password**.
- `mtd_root=/dev/mtdblock4`, `rootfstype=squashfs`, `loadaddr=0x80008000`,
  `fdtcontroladdr=0x81300000`, `read_kernel` = `sf probe 0:0 ${sf_hz} 0; sf read ${loadaddr} ${kernel_offset} ${kernel_size}`.
- Particiones (A/B): mtd0 spl, mtd1 uboot, mtd2 env, mtd3 logo, mtd4 dtb_a, mtd5 dtb_b,
  mtd6 kernel_a, mtd7 kernel_b, mtd8 rootfs_a, mtd9 rootfs_b, mtd10 oem_a, mtd11 oem_b,
  **mtd12 data (39 MB)**, mtd13 factory.

---

## Vía A (PRIMERA opción — la más simple): login por consola serie
El `boot.sh` se lanza con `&`, así que `rcS` termina y el **getty de login** aparece en
ttySAK0 aunque la pantalla esté congelada en el logo.

1. Con `screen` abierto, **da corriente** al panel y espera a que arranque.
2. Debe salir `… login:`. Entra como **root** (contraseña del dispositivo de pruebas).
3. Restaura el arranque de fábrica (una de las dos):
   ```sh
   # restaurar el boot.sh que mantiene Tuya:
   cp /tuya/data/custom/boot.sh.tuya.bak /tuya/data/custom/boot.sh
   # — o — desactivar el hook (rc.local cae a Tuya):
   mv /tuya/data/custom/boot.sh /tuya/data/custom/boot.sh.off
   sync
   reboot
   ```
4. Arranca Tuya → vuelven **WiFi + ADB** → acceso normal recuperado.

> Antes de reiniciar, aprovecha para recoger diagnóstico (ver `scripts/diag_arranque.sh`):
> `sh /tuya/data/custom/diag_arranque.sh` o copia/pega sus comandos. Esto nos da los datos
> que faltan (wpa_supplicant, udhcpc, gadget ADB, logs) para dejar el modo reemplazo seguro.

---

## Vía B (si el login no aparece): U‑Boot → init=/bin/sh (root sin password)
1. Da corriente y **machaca una tecla** en el terminal durante el primer segundo para caer en `anyka$`.
2. Arranca con shell directa (reusa todas las variables correctas, solo cambia `init`):
   ```
   setenv boot_sh 'env set bootargs console=ttySAK0,115200n8 root=${mtd_root} rootfstype=squashfs init=/bin/sh ${mtdparts} ${mem} ${memsize} ${cryptfs}; run read_kernel; bootm ${loadaddr} - ${fdtcontroladdr}'
   run boot_sh
   ```
   (NO uses `saveenv`: es de un solo uso.)
3. Ya con `#` (root, sin login). El rootfs es squashfs (solo lectura) y `/tuya/data` NO está
   montado. Monta la partición **data (mtd12)** y restaura:
   ```sh
   mkdir -p /mnt/d
   mount -t yaffs2 /dev/mtdblock12 /mnt/d   # si falla: probar -t jffs2 o -t ubifs
   ls /mnt/d/custom/                         # ver boot.sh y boot.sh.tuya.bak
   cp /mnt/d/custom/boot.sh.tuya.bak /mnt/d/custom/boot.sh
   sync
   ```
4. **Corta y da corriente** (power‑cycle). Arranca Tuya → WiFi + ADB de vuelta.

---

## Después de recuperar
1. Ejecutar `scripts/diag_arranque.sh` en el panel (por SSH/ADB) y traer su salida.
2. Con esos datos, finalizar `boot.replace.safe.sh` (WiFi+ADB+SSH+UI con fallback a Tuya).
3. Probar el nuevo modo reemplazo **con el UART puesto** (reinicio real): si falla, se
   recupera al instante por serie. Solo entonces se da E3-T01 por validado.
