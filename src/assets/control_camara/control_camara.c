//Librerias de C
#include <string.h>

//Librerias ESP32
#include <esp_log.h>            // MENSAJES DE DEPURACION (ESP_LOGI, ESP_LOGE, ESP_LOGW)
#include "freertos/FreeRTOS.h"  // SISTEMA OPERATIVO EN TIEMPO REAL (MULTITAREA)
#include "freertos/task.h"      // MANEJO DE TAREAS (vTaskDelay, xTaskCreate)

//Librerias Propias
#include "control_camara.h"

//Pines PLACA ESP32-S3-N16R8 WROOM
#define CAM_PWDN    -1  // MODO BAJO CONSUMO (-1 = NO CONECTADO, SIEMPRE ACTIVA)
#define CAM_RESET   -1  // RESET POR HARDWARE (-1 = NO CONECTADO, USA RESET POR SOFTWARE)
#define CAM_SIOD    4   // SCCB DATA  - LEE Y ESCRIBE REGISTROS INTERNOS DE LA CAMARA
#define CAM_SIOC    5   // SCCB CLOCK - SINCRONIZA LA COMUNICACION CON SIOD
#define CAM_HREF    7   // HORIZONTAL SYNC - INDICA CUANDO LA FILA ACTUAL TIENE PIXELES VALIDOS
#define CAM_VSYNC   6   // VERTICAL SYNC - INDICA EL INICIO DE UN NUEVO FRAME COMPLETO
#define CAM_XCLK    15  // MASTER CLOCK - ESP32 GENERA Y ENVIA EL RELOJ PRINCIPAL A LA CAMARA
#define CAM_PCLK    13  // PIXEL CLOCK - LA CAMARA PULSA ESTE PIN POR CADA PIXEL LISTO PARA LEER
#define CAM_Y9      16  // BIT D8 MAS SIGNIFICATIVO
#define CAM_Y8      17  // BIT D7
#define CAM_Y7      18  // BIT D6
#define CAM_Y6      12  // BIT D5
#define CAM_Y5      10  // BIT D4
#define CAM_Y4      8   // BIT D3
#define CAM_Y3      9   // BIT D2
#define CAM_Y2      11  // BIT D1 MENOS SIGNIFICATIVO

#define IMG_WIDTH   224
#define IMG_HEIGHT  224

static const char *component = "control_camara"; //Usado en LOGS

//Estructura con la configuracion de pines y modo de la camara
static camera_config_t camera_config = {
    .pin_pwdn     = CAM_PWDN,
    .pin_reset    = CAM_RESET,
    .pin_xclk     = CAM_XCLK,
    .pin_sccb_sda = CAM_SIOD,
    .pin_sccb_scl = CAM_SIOC,
    .pin_d7       = CAM_Y9,
    .pin_d6       = CAM_Y8,
    .pin_d5       = CAM_Y7,
    .pin_d4       = CAM_Y6,
    .pin_d3       = CAM_Y5,
    .pin_d2       = CAM_Y4,
    .pin_d1       = CAM_Y3,
    .pin_d0       = CAM_Y2,
    .pin_vsync    = CAM_VSYNC,
    .pin_href     = CAM_HREF,
    .pin_pclk     = CAM_PCLK,

    .xclk_freq_hz = 20000000,           // Frecuencia
    .ledc_timer   = LEDC_TIMER_0,       // GENERA LA SEÑAL DEL XCLK
    .ledc_channel = LEDC_CHANNEL_0,     // CANAL DE LA SEÑAL XCLK

    .pixel_format = PIXFORMAT_RGB565,   // FORMATO DEL PIXEL
    .frame_size   = FRAMESIZE_QVGA,     // TAMAÑO DEL FRAME
    .fb_count     = 1,                  // BUFFERS 1(CAPTURA PROCESA LIBERA CAPTURA) 2(PROCESA Y CAPTURA A LA VEZ)
    .fb_location  = CAMERA_FB_IN_PSRAM, // PROCESA EN LA RAM EXTERNA DEL ESP32, DRAM RAM INTERNA
    .grab_mode    = CAMERA_GRAB_WHEN_EMPTY, //CAPTURA SIN SOBRE ESCRIBIR EL BUFFER ACTUAL
};

// inicializa la camara
static esp_err_t init_camera(void){
    esp_err_t err = esp_camera_init(&camera_config); //Coloca las configuraciones a la camara
    
    //Evalua la respuesta de la camara
    if(err != ESP_OK){
        ESP_LOGE(component, "Fallo de grabedad la camara: %s (0x%x)", esp_err_to_name(err), err);
        return err;
    }
    //Envia mensaje de que se inicializo la camara correctamente
    ESP_LOGI(component, "Camara iniciada correctamente");
    return ESP_OK;
}

static void resize_rgb565_to_rgb888(const uint16_t *src, int src_w, int src_h, uint8_t *dst) {
    int x_ratio = (src_w << 16) / IMG_WIDTH + 1;
    int y_ratio = (src_h << 16) / IMG_HEIGHT + 1;

    for (int j = 0; j < IMG_HEIGHT; j++) {
        int y = (j * y_ratio) >> 16;

        for (int i = 0; i < IMG_WIDTH; i++) {
            int x = (i * x_ratio) >> 16;

            uint16_t pixel = src[y * src_w + x];
            pixel = (pixel >> 8) | (pixel << 8); 

            uint8_t r = (pixel >> 11) & 0x1F;
            uint8_t g = (pixel >> 5)  & 0x3F;
            uint8_t b = pixel & 0x1F;

            r = (r << 3) | (r >> 2); 
            g = (g << 2) | (g >> 4); 
            b = (b << 3) | (b >> 2); 

            int idx = (j * IMG_WIDTH + i) * 3;
            dst[idx]     = r;
            dst[idx + 1] = g;
            dst[idx + 2] = b;
        }
    }
}

// Toma la foto y devuelve estadisticas
esp_err_t get_picture(uint8_t **out_rgb888, size_t *out_size) {
    esp_err_t error = init_camera();
    if (error != ESP_OK){
        return error;
    }
    ESP_LOGI(component, "Obteniendo imagen...");
    camera_fb_t *pic = esp_camera_fb_get();
    if (pic == NULL) {
        ESP_LOGE(component, "Frame buffer nulo");
        return ESP_ERR_CAMERA_BASE;
    }

    // Reservar buffer de salida RGB888
    size_t buf_size = IMG_WIDTH * IMG_HEIGHT * 3;
    *out_rgb888 = (uint8_t *)malloc(buf_size);

    if (*out_rgb888 == NULL) {
        ESP_LOGE(component, "Sin memoria para buffer RGB888");
        esp_camera_fb_return(pic);
        return ESP_ERR_NO_MEM;
    }

    // Resize + conversión RGB565 → RGB888
    resize_rgb565_to_rgb888(
        (const uint16_t *)pic->buf,
        pic->width,
        pic->height,
        *out_rgb888
    );

    if (out_size != NULL) {
        *out_size = buf_size;
    }

    // Liberar frame buffer de la cámara — ya no se necesita
    esp_camera_fb_return(pic);
    esp_camera_deinit();

    ESP_LOGI(component, "Imagen transofromada: %dx%d (%d bytes)", IMG_HEIGHT, IMG_WIDTH, buf_size);
    return ESP_OK;
}