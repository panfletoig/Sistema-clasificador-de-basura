#ifndef MEMORY_C
#define MEMORY_C
//Librerias de ESP
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_log.h"

static const char *component = "MEMORY";

void erase_memory(){
    ESP_LOGI(component, "INICIALIZANDO FLASH");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_LOGI(component, "BORRANDO E INICIALIZANDO FLASH");
        nvs_flash_erase();
        nvs_flash_init();
    }
}    
#endif