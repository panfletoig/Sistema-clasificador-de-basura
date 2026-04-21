#ifdef MODULO_IA
#include <stdio.h>
#include "esp_system.h"
#include "esp_log.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_task_wdt.h>
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include <stdlib.h>
#include <string.h>
#include <esp_timer.h>
#include "../src/assets/modelo/modelo.h"
#include "esp_heap_caps.h"

#define IMAGE_WIDTH 96
#define IMAGE_HEIGHT 96
#define MODEL_TAG "MODELS"

constexpr int kTensorArenaSize = 512 * 1024;

static uint8_t* tensor_arena = nullptr;

// También el intérprete, usando heap en PSRAM:
static tflite::MicroMutableOpResolver<12> micro_op_resolver;  // global

// Declaración de los tensores y el intérprete
const tflite::Model *model = nullptr;
tflite::MicroInterpreter *interpreter = nullptr;
TfLiteTensor *input = nullptr;
TfLiteTensor *output = nullptr;

TfLiteStatus RegisterOps(tflite::MicroMutableOpResolver<12> &op_resolver)
{
    TF_LITE_ENSURE_STATUS(op_resolver.AddFullyConnected());
    TF_LITE_ENSURE_STATUS(op_resolver.AddQuantize());
    TF_LITE_ENSURE_STATUS(op_resolver.AddReshape());
    TF_LITE_ENSURE_STATUS(op_resolver.AddConv2D());
    TF_LITE_ENSURE_STATUS(op_resolver.AddMaxPool2D());
    TF_LITE_ENSURE_STATUS(op_resolver.AddSoftmax());
    TF_LITE_ENSURE_STATUS(op_resolver.AddDequantize());
    TF_LITE_ENSURE_STATUS(op_resolver.AddMul());
    TF_LITE_ENSURE_STATUS(op_resolver.AddAdd());
    TF_LITE_ENSURE_STATUS(op_resolver.AddMean());
    TF_LITE_ENSURE_STATUS(op_resolver.AddHardSwish());
    TF_LITE_ENSURE_STATUS(op_resolver.AddDepthwiseConv2D());
    return kTfLiteOk;
}

// Preprocesar la imagen: normalizar los valores de los píxeles
void preprocess_image(uint8_t *raw_data, int8_t *image_data, bool is_int8)
{
    for (int i = 0; i < IMAGE_WIDTH * IMAGE_HEIGHT * 3; i++) {
        if (is_int8) {
            image_data[i] = static_cast<int8_t>(raw_data[i] - 128);
        } else {
            image_data[i] = static_cast<uint8_t>(raw_data[i]);
        }
    }
}

void printPredictedClass(char* result)
{
    int output_size = 4;
    int8_t max_value = -128;
    int prediccion = -1;

    for (int i = 0; i < output_size; i++)
    {
        int8_t value = output->data.int8[i];
        if (value > max_value)
        {
            max_value = value;
            prediccion = i;
        }
    }

    if (prediccion != -1)
    {
        *result = '0' + prediccion; 
        ESP_LOGI(MODEL_TAG, "The image belongs to letter: %d", prediccion);
    }
    else
    {
        *result = '\0'; 
        ESP_LOGE(MODEL_TAG, "No se pudo predecir.");
    }
}

void run_inference(const unsigned char *model_data, uint8_t* input_buffer, char* result)
{
    if (tensor_arena == nullptr) {
        tensor_arena = (uint8_t*) heap_caps_malloc(kTensorArenaSize, MALLOC_CAP_SPIRAM);
        if (tensor_arena == nullptr) {
            ESP_LOGE(MODEL_TAG, "Fallo al alojar tensor_arena en PSRAM");
            return;
        }
        ESP_LOGI(MODEL_TAG, "tensor_arena: %d KB en PSRAM", kTensorArenaSize / 1024);
    }
    // Cargar el modelo
    int64_t start_time = esp_timer_get_time();
    model = tflite::GetModel(model_data);
    int64_t end_time = esp_timer_get_time();
    ESP_LOGI(MODEL_TAG, "Model load time: %.6f s", (end_time - start_time)/ 1000000.0);
    if (model->version() != TFLITE_SCHEMA_VERSION)
    {
        ESP_LOGE(MODEL_TAG, "Model schema version mismatch");
        return;
    }
    
    RegisterOps(micro_op_resolver);
    
    tflite::MicroInterpreter static_interpreter(model, micro_op_resolver, tensor_arena, kTensorArenaSize);
    interpreter = &static_interpreter;
    
    // Asignar tensores
    if (interpreter->AllocateTensors() != kTfLiteOk)
    {
        ESP_LOGE(MODEL_TAG, "Failed to allocate tensors.");
        return;
    }
    
    // Obtener tensores de entrada y salida
    input = interpreter->input(0);
    if (input == nullptr) {
        ESP_LOGE(MODEL_TAG, "input es nullptr — modelo incompatible o arena insuficiente");
        return;
    }

    output = interpreter->output(0);
    if (output == nullptr) {
        ESP_LOGE(MODEL_TAG, "output es nullptr");
        return;
    }
    bool is_int8 = (input->type == kTfLiteInt8);
    
    // Preprocesar la imagen
    int8_t *image_data = (int8_t*) heap_caps_malloc(IMAGE_WIDTH * IMAGE_HEIGHT * 3, MALLOC_CAP_SPIRAM);
    if (image_data == nullptr) {
        ESP_LOGE(MODEL_TAG, "Fallo al alojar image_data — PSRAM insuficiente");
        return;
    }
    if (input_buffer == NULL) {
        ESP_LOGE(MODEL_TAG, "Fallo al alojar image_data — PSRAM insuficiente");
        return;
    }

    preprocess_image(input_buffer, image_data, is_int8);
    
    // Copiar datos preprocesados al tensor de entrada
    memcpy(input->data.int8, image_data, sizeof(image_data));
    free(image_data); // ✅ liberar antes de invocar, ya no se necesita
    
    // Ejecutar la inferencia
    start_time = esp_timer_get_time();
    interpreter->Invoke();
    end_time = esp_timer_get_time();
    ESP_LOGI(MODEL_TAG, "Inference time: %.6f s", (end_time - start_time)/ 1000000.0);
    // Manejar la salida del modelo
    printPredictedClass(result);
}

extern "C" void run_model(uint8_t* input_buffer, char* result)
{
    run_inference(modelo_tflite, input_buffer, result);
}
#endif