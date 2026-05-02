#pragma once
#include "esp_err.h"
esp_err_t servo_init(void);
esp_err_t servo_x_mover(int adc_val);
esp_err_t servo_z_mover(int adc_val);