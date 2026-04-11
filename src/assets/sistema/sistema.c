//Librerias ESP32
#include "esp_chip_info.h" //Infomacion del chip
#include "esp_log.h"       //Sistema de logs

static const char* component = "sistema";

//Imprime la informacion del chip ESP32-S3
void info_sistema() {
    esp_chip_info_t chip_info;  //Variable para almacenar la informacion del CHIP
    esp_chip_info(&chip_info);  //Obtiene la informacion del chip

    ESP_LOGI(component, "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    ESP_LOGI(component, "Chip....: %s", CONFIG_IDF_TARGET);
    ESP_LOGI(component, "Núcleos.: %d", chip_info.cores);
    ESP_LOGI(component, "Revisión: %d", chip_info.revision);
    ESP_LOGI(component, "Flash...: %s",(chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "Embebida" : "Externa");
    ESP_LOGI(component, "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
}