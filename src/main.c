//Librerias de ESP32
#include "esp_log.h"            //Informacion por medio de LOGS
#include "esp_sleep.h"          //Biblioteca para DEEP SLEEP
#include "driver/gpio.h"        //Driver de pines GPIO
#include "esp_heap_caps.h"
#include "esp_task_wdt.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"

//Libreria propias
#include "assets/control_camara/control_camara.h"   //Control de la camara
#include "assets/wifi_connection/wifi_connection.h" //Control de conexion WIFI
#include "assets/memoria/memoria.h"                 //Control de displays de memoria
#include "assets/sistema/sistema.h"                 //Control de display de sistema
#ifdef MODULO_IA
#include "assets/computer-vision/vision_tflite.h"
#endif
#include "assets/almacenamiento/almacenamiento.h"
#define GPIO_WAKEUP GPIO_NUM_21

/* FUNCIONES */
void control_IA(uint8_t *out);

static const char* component = "app";

static RTC_DATA_ATTR uint8_t COMMAREA = 0b00000001;

void app_main(void){
    ESP_LOGI(component, "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause(); //Causas de wakeup
    if (wakeup_cause == ESP_SLEEP_WAKEUP_EXT0) {
        //Si desperto por pin GPIO o despertar externo
        ESP_LOGI(component, "Despertó por GPIO %d", GPIO_WAKEUP);
    } 
    
    // Configurar pin con pulldown interno
    gpio_pulldown_en(GPIO_WAKEUP);  //Activa la resistencia que coloca a 0V
    gpio_pullup_dis(GPIO_WAKEUP);   //Desactiva la resistencia que coloca a 3.3V
    
    //Si es la primera ejeccuion
    if(COMMAREA & 1){
        ESP_LOGI(component, "Inicio aplicacion");   //Coloca en el log el inicio de la aplicacion
        info_sistema();                             //Imprime la informacion del sistema
        info_memoria("INIT");                       //Imprime la informacion de memoria inicialmente
        COMMAREA = COMMAREA >> 1;                   //Mueve 1 al ultimo bit para solo ejecutar la primera vez
        ESP_LOGI(component, "Commarea: %d", COMMAREA);  
    }
    
    esp_err_t err = inicializar_memoria_flash();    //Inicializa la FLASH
    if (ESP_OK != err){
        //Si hubo un error entonces lo reporta y termina
        ESP_LOGE(component, "Error inicializando memoria: %s", esp_err_to_name(err));
        return;
    }
    info_memoria("FLASH");  //Imprime la informacion de la memoria flash
    
    //establecer_conexion();  //Establece la conexion con en la red
    info_memoria("WIFI");   //Imprime informacion de la memoria flash luego de la conexion
    
    //Control de la camara              
    uint8_t *out = NULL;
    size_t out_size = 0;
    
    if(ESP_OK != get_picture(&out, &out_size)) { 
        return;    
    }

    control_IA(out);
    
    info_memoria("FINAL");
    ESP_LOGI(component, "Modo Deep Sleep");
    ESP_LOGI(component, "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    // Despertar cuando el pin sube a HIGH
    mount_sd();
    free(out);
    out = NULL;
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_sleep_enable_ext0_wakeup(GPIO_WAKEUP, 1);
    esp_deep_sleep_start();
}

void control_IA(uint8_t *out){
    #ifdef MODULO_IA
    char resultado;
    run_model(out, &resultado);
    if(resultado == '1'){
        resultado = 'O';
    }
    else if(resultado == '2'){
        resultado = 'C';
    }
    else if(resultado == '3'){
        resultado = 'P';
    }
    else{
        resultado = 'N';
    }
    ESP_LOGI(component, "Resultado inferencia: %c", resultado);   
    #endif
    return;
}