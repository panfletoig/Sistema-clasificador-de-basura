#pragma once

esp_err_t sensor_peso_init(void);
esp_err_t sensor_calibrar_offset(void);
esp_err_t sensor_calibrar_escala(void);
float sensor_peso_leer(void);