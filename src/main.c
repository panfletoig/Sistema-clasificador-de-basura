//Librerias de ESP32
#include "esp_log.h"            //Informacion por medio de LOGS
#include "esp_sleep.h"          //Biblioteca para DEEP SLEEP
#include "driver/gpio.h"        //Driver de pines GPIO
#include "esp_heap_caps.h"
#include "esp_task_wdt.h"

//Libreria propias
#include "assets/control_camara/control_camara.h"   //Control de la camara
#include "assets/wifi_connection/wifi_connection.h" //Control de conexion WIFI
#include "assets/memoria/memoria.h"                 //Control de displays de memoria
#include "assets/sistema/sistema.h"                 //Control de display de sistema
#include "assets/computer-vision/vision_tflite.h"

#define GPIO_WAKEUP GPIO_NUM_21

static const char* component = "app";

static RTC_DATA_ATTR uint8_t COMMAREA = 0b00000001;

void save_as_bmp(const char *filename, uint8_t *rgb, int width, int height);

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

    save_as_bmp("/sdcard/foto.bmp", out, 96, 96);
    
    info_memoria("FINAL");
    ESP_LOGI(component, "Modo Deep Sleep");
    ESP_LOGI(component, "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    // Despertar cuando el pin sube a HIGH
    free(out);
    out = NULL;
    esp_sleep_enable_ext0_wakeup(GPIO_WAKEUP, 1);
    esp_deep_sleep_start();
}

void save_as_bmp(const char *filename, uint8_t *rgb, int width, int height)
{
    FILE *f = fopen(filename, "wb");
    if (!f) return;

    int filesize = 54 + 3 * width * height;

    uint8_t fileHeader[14] = {
        'B','M',  // tipo BMP
        filesize, filesize>>8, filesize>>16, filesize>>24,
        0,0, 0,0,
        54,0,0,0
    };

    uint8_t infoHeader[40] = {
        40,0,0,0,
        width, width>>8, width>>16, width>>24,
        height, height>>8, height>>16, height>>24,
        1,0,
        24,0
    };

    fwrite(fileHeader, 1, 14, f);
    fwrite(infoHeader, 1, 40, f);

    // BMP guarda en BGR y de abajo hacia arriba
    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            int i = (y * width + x) * 3;

            uint8_t r = rgb[i];
            uint8_t g = rgb[i + 1];
            uint8_t b = rgb[i + 2];

            uint8_t pixel[3] = {b, g, r}; // RGB → BGR
            fwrite(pixel, 1, 3, f);
        }
    }

    fclose(f);
}