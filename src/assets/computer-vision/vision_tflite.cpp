#ifdef MODULO_IA
//Librerias C
#include <stdio.h>

//Librerias ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_task_wdt.h>
#include <esp_timer.h>
#include <stdlib.h>
#include <string.h>
#include "esp_system.h"
#include "esp_heap_caps.h"

//Librerias de terceros 
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"

//Modelo de IA
#include "../src/assets/modelo/modelo.h"
#include "../src/assets/computer-vision/vision_tflite.h"

#define ANCHO_IMAGEN 224      //Ancho de imagen
#define ALTO_IMAGEN  224      //Alto de imagen

const static char* componente = "vision_tlflite"; //Nombre del componente
constexpr int kTensorArenaSize = 700 * 1024;     //Area de memoria para tensores
static uint8_t* tensor_arena = nullptr;           //Memoria de longitud KTensorArenaSize

//Carga las operaciones permitidas para el modelo
TfLiteStatus RegisterOps(tflite::MicroMutableOpResolver<8> &op_resolver)
{
    TF_LITE_ENSURE_STATUS(op_resolver.AddAdd());                
    TF_LITE_ENSURE_STATUS(op_resolver.AddMul());                
    TF_LITE_ENSURE_STATUS(op_resolver.AddConv2D());               
    TF_LITE_ENSURE_STATUS(op_resolver.AddHardSwish());               
    TF_LITE_ENSURE_STATUS(op_resolver.AddDepthwiseConv2D());               
    TF_LITE_ENSURE_STATUS(op_resolver.AddMean());               
    TF_LITE_ENSURE_STATUS(op_resolver.AddFullyConnected());               
    TF_LITE_ENSURE_STATUS(op_resolver.AddSoftmax());               
    return kTfLiteOk;
}

void preprocesa_imagen(uint8_t *raw_data, int8_t *image_data)
{
    int total_pixels = ANCHO_IMAGEN * ALTO_IMAGEN;
    const float input_scale = 0.007843f;
    const int   input_zp    = -1;
    
    for (int i = 0; i < total_pixels; i++) {
        for (int c = 0; c < 3; c++) {
            float norm = (float)raw_data[i * 3 + c] / 127.5f - 1.0f;
            int val = (int)roundf(norm / input_scale) + input_zp;
            if (val < -128) val = -128;
            if (val >  127) val =  127;
            image_data[i * 3 + c] = (int8_t)val;
        }
    }
}

void printPredictedClass(char* resultado, TfLiteTensor* output)
{
    int   prediccion = -1;
    float mejor_prob = -1.0f;
    const char* nombres[] = {"NoAprobechable", "Organicos", "Papel-Carton", "Plasticos"};

    float out_scale = (*output).params.scale;
    int   out_zp    = (*output).params.zero_point;

    ESP_LOGI(componente, "Output type=%d scale=%.8f zp=%d",
        (*output).type, out_scale, out_zp);

    for (int i = 0; i < 4; i++) {
        int8_t raw = (*output).data.int8[i];
        float prob = (raw - out_zp) * out_scale;  // siempre INT8
        ESP_LOGI(componente, "Clase[%d]=%s raw=%d prob=%.4f", i, nombres[i], raw, prob);

        if (prob > mejor_prob) {
            mejor_prob = prob;
            prediccion = i;
        }
    }

    if (mejor_prob < 0.5f) {
        *resultado = '\0';
        ESP_LOGW(componente, "Confianza insuficiente: %.2f%%", mejor_prob * 100);
        return;
    }

    *resultado = '0' + prediccion;
    ESP_LOGI(componente, "Clase: %s (%.2f%%)", nombres[prediccion], mejor_prob * 100);
}

/****************************************************************
 * Realiza la inferencia cargando el modelo, preprocesando la im-
 * agen y realizando la inferencia dejando el resultado de forma-
 * interna en el puntero output 
 *****************************************************************/
esp_err_t realiza_inferencia(uint8_t* input_buffer, char* resultado)
{
    const unsigned char *model_data = modelo_tflite;

    if (tensor_arena == nullptr) {
        tensor_arena = (uint8_t*) heap_caps_malloc(kTensorArenaSize, MALLOC_CAP_SPIRAM);
        if (tensor_arena == nullptr) {
            ESP_LOGE(componente, "Fallo al alojar tensor_arena en PSRAM");
            return ESP_ERR_NO_MEM;
        }
    }

    tflite::MicroMutableOpResolver<8> op_resolver;
    TF_LITE_ENSURE_STATUS(op_resolver.AddAdd());
    TF_LITE_ENSURE_STATUS(op_resolver.AddMul());
    TF_LITE_ENSURE_STATUS(op_resolver.AddConv2D());
    TF_LITE_ENSURE_STATUS(op_resolver.AddHardSwish());
    TF_LITE_ENSURE_STATUS(op_resolver.AddDepthwiseConv2D());
    TF_LITE_ENSURE_STATUS(op_resolver.AddMean());
    TF_LITE_ENSURE_STATUS(op_resolver.AddFullyConnected());
    TF_LITE_ENSURE_STATUS(op_resolver.AddSoftmax());

    const tflite::Model *model = tflite::GetModel(model_data);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        ESP_LOGE(componente, "Model schema version mismatch");
        return ESP_ERR_NOT_SUPPORTED;
    }
z
    // ✅ Limpiar arena antes de cada uso
    memset(tensor_arena, 0, kTensorArenaSize);

    // ✅ Interpreter en heap — no en stack
    tflite::MicroInterpreter* interp = new tflite::MicroInterpreter(
        model, op_resolver, tensor_arena, kTensorArenaSize);

    if (interp->AllocateTensors() != kTfLiteOk) {
        ESP_LOGE(componente, "Failed to allocate tensors.");
        delete interp;
        return ESP_ERR_NO_MEM;
    }

    TfLiteTensor *input     = interp->input(0);
    TfLiteTensor *output    = interp->output(0);

    if (input == nullptr || output == nullptr) {
        ESP_LOGE(componente, "input o output nullptr");
        delete interp;
        return ESP_ERR_NOT_SUPPORTED;
    }

    // Preprocesar y copiar imagen
    int8_t *image_data = (int8_t*) heap_caps_malloc(
        ANCHO_IMAGEN * ALTO_IMAGEN * 3, MALLOC_CAP_SPIRAM);
    if (image_data == nullptr) {
        delete interp;
        return ESP_ERR_NO_MEM;
    }

    preprocesa_imagen(input_buffer, image_data);
    memcpy(input->data.int8, image_data, ANCHO_IMAGEN * ALTO_IMAGEN * 3);
    heap_caps_free(image_data);

    int64_t t0 = esp_timer_get_time();
    interp->Invoke();
    ESP_LOGI(componente, "Inference time: %.3f s", 
             (esp_timer_get_time() - t0) / 1000000.0);

    printPredictedClass(resultado, output);

    delete interp;
    return ESP_OK;
}

/**********************************************************************
 * funcion externada a C para usar el modelo entrenado, requiere el bu-
 * ffer de imagen(imagen de 3 canales) transformado a un array unidime-
 * nsional y colocando el resultado de la prediccion en resultado
***********************************************************************/
extern "C" esp_err_t ejecuta_modelo(uint8_t* input_buffer, char* resultado)
{
    return realiza_inferencia(input_buffer, resultado);
}
#endif