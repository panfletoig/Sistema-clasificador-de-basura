//Librerias de ESP32
#include <nvs_flash.h>          // ALMACENAMIENTO NO VOLATIL (GUARDA DATOS AL APAGAR)
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_log.h"

//Libreria propias
#include "assets/control_camara/control_camara.h"

static const char* component = "app";
void print_memory_info();

void app_main(void){
    print_memory_info("INIT");

    //Inicializa la FLASH
    nvs_flash_init();
    print_memory_info("FLASH");

    //Inicializa la camara
    if(ESP_OK != init_camera()) return;
    print_memory_info("INIT-CAM");

    //Toma la foto
    color_stats_t stats = {0};          //Creamos la estructura 
    if(ESP_OK != get_picture(&stats)) return; //Retorna las estadisticas de la foto 
    print_memory_info("STATS");


    imprimir_resultado(&stats);
    print_memory_info("PRINT-STATS");
}

void print_memory_info(char* titulo) {
    ESP_LOGI(component, "━━━━━━━━━━━━━━ START-%s ━━━━━━━━━━━━━━", titulo);
    // Memoria heap libre total
    ESP_LOGI(component, "Heap libre: %lu bytes", esp_get_free_heap_size());
    // Mínimo histórico (el menor valor que ha tenido)
    ESP_LOGI(component, "Heap mínimo histórico: %lu bytes", esp_get_minimum_free_heap_size());
    // Memoria interna (DRAM)
    ESP_LOGI(component, "DRAM libre: %d bytes", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    // Memoria PSRAM (si tienes módulo con PSRAM como el WROVER)
    ESP_LOGI(component, "PSRAM libre: %d bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    // Bloque contiguo más grande disponible
    ESP_LOGI(component, "Bloque más grande: %d bytes", heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
    ESP_LOGI(component, "━━━━━━━━━━━━━━ ENDOF-%s ━━━━━━━━━━━━━━", titulo);
}