#ifdef MODULO_IA
#pragma once
#include <stdint.h>
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

extern esp_err_t ejecuta_modelo(uint8_t* input_buffer, char* resultado);

#ifdef __cplusplus
}
#endif
#endif
