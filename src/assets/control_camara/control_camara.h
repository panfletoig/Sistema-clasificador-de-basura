#pragma once

//Librerias C
#include <stdio.h>

//Libreria ESP32
#include "../components/esp32-camera/driver/include/esp_camera.h" //LIBRERIA DE CAMARA

//Estructuras
typedef struct {
    uint32_t rojos;
    uint32_t verdes;
    uint32_t azules;
    uint32_t total;
} color_stats_t;

//Funciones
esp_err_t init_camera(void);
esp_err_t get_picture(color_stats_t *stats);
void imprimir_resultado(color_stats_t* stats);
