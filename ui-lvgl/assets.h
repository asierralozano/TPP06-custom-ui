#ifndef ASSETS_H
#define ASSETS_H
/* Ruta LVGL (con letra de unidad 'A') de un asset relativo a la carpeta img/ de Tuya.
   Base: variable de entorno ASSET_DIR si existe, si no el default compilado.
   Ej: asset("weather/36/sunny.png") -> "A:/.../img/weather/36/sunny.png". */
const char *asset(const char *rel);
#endif
