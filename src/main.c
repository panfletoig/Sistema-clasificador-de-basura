//Librerias de ESP32
#include <nvs_flash.h>          // ALMACENAMIENTO NO VOLATIL (GUARDA DATOS AL APAGAR)

//Libreria propias
#include "assets/control_camara/control_camara.h"

void app_main(void){
    //Inicializa la FLASH
    nvs_flash_init();

    //Inicializa la camara
    if(ESP_OK != init_camera()) return;
    
    //Toma la foto
    color_stats_t stats = {0};          //Creamos la estructura 
    if(ESP_OK != get_picture(&stats)) return; //Retorna las estadisticas de la foto 
    
    imprimir_resultado(&stats);
}