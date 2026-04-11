//Librerias de ESP
#include "nvs_flash.h"          //Almacenamiento no volatil                            
#include "esp_heap_caps.h"      //Funciones para la gestion de memoria
#include "esp_system.h"         //Funciones generales del sistema
#include "esp_log.h"            //Sistema de LOG

//Libreria propia
#include "memoria.h"

static const char *component = "memory"; //Componentes de la memoria

esp_err_t inicializar_memoria_flash(){
    ESP_LOGI(component, "Inicializando flash");                 //Escribe al LOG
    esp_err_t err = nvs_flash_init();                           //Inicializa Memoria Flash
    /* Si no hay paginas disponibles */
    if (ESP_ERR_NVS_NO_FREE_PAGES == err) {
        //Borra la memoria flash
        ESP_LOGW(component, "Borrando e inicializando flash");
        err = nvs_flash_erase();
        //Si hubo un error retorna el error
        if(ESP_OK != err){        
            return err;
        }
        return nvs_flash_init(); //Inicializa la flash y retorna el error
    }
    return err;
}

//Impresiones de memoria
void info_memoria(char* titulo) {
    ESP_LOGI(component, "━━━━━━━━━━━━━━ S-%s ━━━━━━━━━━━━━━", titulo);
    // Memoria heap libre total
    ESP_LOGI(component, "Heap libre: %lu KB", esp_get_free_heap_size() / 1024);
    // Mínimo histórico (el menor valor que ha tenido)
    ESP_LOGI(component, "Heap mínimo histórico: %lu KB", esp_get_minimum_free_heap_size() / 1024);
    // Memoria interna (DRAM)
    ESP_LOGI(component, "DRAM libre: %d KB", heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024);
    // Memoria PSRAM (si tienes módulo con PSRAM como el WROVER)
    ESP_LOGI(component, "PSRAM libre: %d KB", heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024);
    // Bloque contiguo más grande disponible
    ESP_LOGI(component, "Bloque más grande: %d KB", heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT) / 1024);
    ESP_LOGI(component, "━━━━━━━━━━━━━━ E-%s ━━━━━━━━━━━━━━", titulo);
}