//Librerias de ESP32
#include <esp_timer.h>          //Modulo de tiempo del ESP32
#include "esp_log.h"            //Informacion por medio de LOGS
#include "esp_sleep.h"          //Biblioteca para DEEP SLEEP
#include "driver/i2c_master.h"  //Driver de pines GPIO

//Librerías de ESPRESSIF    
#include "freertos/FreeRTOS.h"      //Nucleo del sistema operativo FreeRTOS (requerido por todas las demás librerías de FreeRTOS)
#include "freertos/task.h"          //Creación y manejo de tareas en FreeRTOS
#include "freertos/event_groups.h"  //Grupos de eventos para sincronización entre tareas

//Libreria propias
#include "assets/control_camara/control_camara.h"   //Control de la camara
#include "assets/wifi_connection/wifi_connection.h" //Control de conexion WIFI
#include "assets/memoria/memoria.h"                 //Control de displays de memoria
#include "assets/sistema/sistema.h"                 //Control de display de sistema
#include "assets/almacenamiento/almacenamiento.h"   //Control de almacenamiento en SD
#include "assets/servo/servo.h"
#include "assets/sensor_peso/sensor_peso.h"
#ifdef MODULO_IA
#include "assets/computer-vision/vision_tflite.h"
#endif

#define GPIO_WAKEUP GPIO_NUM_19

/* Metodos */
void proceso();
void conexion_wifi();
void control_IA(uint8_t *out);

/* Funciones */
esp_err_t init_config();

/* Variables */
static const char* componente = "app_main";     //Nombre del modulo principal
static RTC_DATA_ATTR uint8_t COMMAREA = 0;      //Control de ejeccion

/* metodo principal */
void app_main(void){
    int64_t tiempo_inicio = esp_timer_get_time();  //Toma el tiempo para el timestamp
    ESP_LOGI(componente, "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    esp_err_t err = init_config();
    if(ESP_OK == err) {
        esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause(); //Obtiene las causas de depertar del esp32
        if (wakeup_cause == ESP_SLEEP_WAKEUP_EXT0) {   
            ESP_LOGI(componente, "Despertó por GPIO %d", GPIO_WAKEUP);
            proceso();
        } 
    }
    int64_t tiempo_fin = esp_timer_get_time();  //Toma el tiempo para el timestamp
    ESP_LOGI(componente, "Tiempo de ejecuccion: %.6f s", (tiempo_fin - tiempo_inicio) / 1000000.0);
    ESP_LOGI(componente, "Entrando en Deep Sleep");
    ESP_LOGI(componente, "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    esp_sleep_enable_ext0_wakeup(GPIO_WAKEUP, 1); //Cuando el pin sea 1 ejecute
    esp_deep_sleep_start();                       //Ejecuta el deep sleep
}

/***************************** 
 * CONFIGURACIONES INICIALES *
 * - Configuracion de pines  *
 * - Inicializar Flash       *
 * - Setea la flag           *
******************************/
esp_err_t init_config(){
    gpio_pulldown_en(GPIO_WAKEUP);  //Activa la resistencia que coloca a 0V
    gpio_pullup_dis(GPIO_WAKEUP);   //Desactiva la resistencia que coloca a 3.3V
    
    esp_err_t err = inicializar_memoria_flash();    //Inicializa la flash
    if (ESP_OK != err){
        ESP_LOGE(componente, "Error inicializando memoria: %s", esp_err_to_name(err));
    }
    
    //Si es la primera ejeccuion
    if(COMMAREA == 0){
        info_sistema();                             //Imprime la informacion del sistema
        info_memoria("INIT");                       //Imprime la informacion de memoria inicialmente
        COMMAREA = 1;                               //Mueve 1 al ultimo bit para solo ejecutar la primera vez
        ESP_LOGI(componente, "Commarea: %d", COMMAREA);  
        conexion_wifi();
        err = ESP_FAIL;
    }
    info_memoria("FLASH");  //Imprime la informacion de la memoria flash
    return err;
}

/**********************************
 * LLAMADA PRINCIPAL DE FUNCIONES *
 * - CAMARA                       *
 * - CONEXION WIFI                *
 * - PETICIONES HTTP              *
 * - CONTROL DE IA                *
 * - SENSORES                     *
 * - REGISTROS DE MEMORIA         *
 **********************************/
void proceso(){
    servo_init();
    mount_sd();
    conexion_wifi();
    info_memoria("WIFI");   //Imprime informacion de la memoria flash luego de la conexion

    //Control de la camara              
    uint8_t *out = NULL;    //Crea un puntero que esta a nulo
    size_t out_size = 0;    //Asigna el tamaño a 0
    
    ESP_LOGI(componente, "---------------------");
    servo_x_mover(1);
    servo_z_mover(3000);
    servo_z_mover(10);
    servo_z_mover(1500);
    
    ESP_LOGI(componente, "---------------------");
    servo_x_mover(2048);
    servo_z_mover(3000);
    servo_z_mover(10);
    servo_z_mover(1500);
    ESP_LOGI(componente, "---------------------");
    
    if(ESP_OK != get_picture(&out, &out_size)) {
        ESP_LOGE(componente, "error al tomar fotografia");
        return;    
    }
    control_IA(out);
    free(out);
    out = NULL;
    return;
}

void conexion_wifi(){
    establecer_conexion();
    return;
}

void control_IA(uint8_t *out){
    #ifdef MODULO_IA
    char resultado;
    esp_err_t err = ejecuta_modelo(out, &resultado);
    if(err != ESP_OK){
        ESP_LOGE(componente, "Error al correr el modelo %s", esp_err_to_name(err));
        return;
    }
    if(resultado == '0'){
        resultado = 'O';
    }
    else if(resultado == '1'){
        resultado = 'C';
    }
    else if(resultado == '2'){
        resultado = 'P';
    }
    else{
        resultado = 'N';
    }
    ESP_LOGI(componente, "Resultado inferencia: %c", resultado);   
    #endif
    return;
}