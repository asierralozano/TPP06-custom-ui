#!/bin/sh
# ============================================================================
#  TPP06 — Inventario de la NAND (SOLO LECTURA, sin riesgo)
# ============================================================================
#  EJECUTAR EN LA CONSOLA ROOT DEL PANEL.
#  No escribe nada en la NAND. Lista particiones, tamanos y hashes para:
#   - Confirmar el mapa real de particiones de ESTE panel vs el OTA 1.2.3.
#   - Tener huellas antes de cualquier cambio persistente.
#   - Decidir que particiones volcar para recuperacion.
#
#  Mapa esperado del OTA (double-copy, slots A/B):
#    dtb=mtd4/5  kernel=mtd6/7  rootfs(/)=mtd8/9  user(/tuya/app)=mtd10/11
#    data(/tuya/data YAFFS2)=mtd12.  Bootloader/uboot/env/factory: mtd0..mtd3.
# ============================================================================

echo "===== /proc/mtd (particiones reales) ====="
cat /proc/mtd

echo ""
echo "===== Montajes relevantes ====="
grep -E "mtd|tuya|overlay| / " /proc/mounts

echo ""
echo "===== Espacio en /tuya/data (persistente) ====="
df -h /tuya/data 2>/dev/null

echo ""
echo "===== Slot OTA activo y modo ====="
echo "persist.ota.slot = $(nvram get persist.ota.slot)"
echo "persist.ftm.mode = $(nvram get persist.ftm.mode)"

echo ""
echo "===== Hash y tamano de cada mtdblock (solo lectura) ====="
# md5sum de cada /dev/mtdblockN. No modifica nada.
for n in $(grep -o '^mtd[0-9]*' /proc/mtd | sed 's/mtd//'); do
    dev=/dev/mtdblock$n
    if [ -e "$dev" ]; then
        name=$(grep "^mtd$n:" /proc/mtd | sed 's/.*"\(.*\)".*/\1/')
        sz=$(grep "^mtd$n:" /proc/mtd | awk '{print $2}')
        md5=$(md5sum "$dev" 2>/dev/null | awk '{print $1}')
        echo "  mtd$n  \"$name\"  size=0x$sz  md5=$md5"
    fi
done

echo ""
echo "===== Hecho. Copia esta salida al Mac para registrarla. ====="
echo "Para el VOLCADO completo de recuperacion usaremos el puller --via-shell"
echo "sobre /dev/mtdblockN (sin almacenar en el panel, que tiene poco espacio)."
