#pragma once

//Librerias C
#include <stdio.h>

//Libreria ESP32
#include "../components/esp32-camera/driver/include/esp_camera.h" //LIBRERIA DE CAMARA

//Funciones
esp_err_t get_picture(uint8_t **out_rgb888, size_t *out_size);