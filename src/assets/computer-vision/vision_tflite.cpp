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
constexpr int kTensorArenaSize = 700 * 1024;      //Area de memoria para tensores
static uint8_t* tensor_arena = nullptr;           //Memoria de longitud KTensorArenaSize

static tflite::MicroMutableOpResolver<12> micro_op_resolver;  //Operaciones permitidas para el modelo de IA

const tflite::Model *model = nullptr;            //Usado para cargar el modelo
tflite::MicroInterpreter *interpreter = nullptr; //Realiza interpretaciones
TfLiteTensor *input = nullptr;                   //Entrada del modelo
TfLiteTensor *output = nullptr;                  //Salida del modelo

//Carga las operaciones permitidas para el modelo
TfLiteStatus RegisterOps(tflite::MicroMutableOpResolver<12> &op_resolver)
{
    TF_LITE_ENSURE_STATUS(op_resolver.AddDepthwiseConv2D());    //Convoluciones optimizadas
    TF_LITE_ENSURE_STATUS(op_resolver.AddFullyConnected());     //Conecta todas las neuronas de la red
    TF_LITE_ENSURE_STATUS(op_resolver.AddDequantize());         //Optimizaciones
    TF_LITE_ENSURE_STATUS(op_resolver.AddHardSwish());          //Desicion 
    TF_LITE_ENSURE_STATUS(op_resolver.AddMaxPool2D());          //Saca valores maximos
    TF_LITE_ENSURE_STATUS(op_resolver.AddQuantize());           //Optimizaciones
    TF_LITE_ENSURE_STATUS(op_resolver.AddReshape());            //Transformaciones
    TF_LITE_ENSURE_STATUS(op_resolver.AddSoftmax());            //Funcion SOFTMAX para probabilidades
    TF_LITE_ENSURE_STATUS(op_resolver.AddConv2D());             //Convoluciones filtros
    TF_LITE_ENSURE_STATUS(op_resolver.AddMul());                //Multiplicar
    TF_LITE_ENSURE_STATUS(op_resolver.AddAdd());                //Sumar
    TF_LITE_ENSURE_STATUS(op_resolver.AddMean());               //Promediar
    return kTfLiteOk;
}

//Normaliza los valores de entrada
void preprocesa_imagen(uint8_t *raw_data, int8_t *image_data)
{
    unsigned long size = ANCHO_IMAGEN * ALTO_IMAGEN * 3; //Ancho X Alto X Canales (R, G, B) 
    for (int i = 0; i < size; i++) {
        //Comprueba si el tipo de modelo es signado o no signado
        if ((*input).type == kTfLiteInt8) {
            image_data[i] = static_cast<int8_t>(raw_data[i] - 128); 
        } else {
            image_data[i] = static_cast<uint8_t>(raw_data[i]);
        }
    }
}

void printPredictedClass(char* resultado)
{
    int output_size = 4;        //4 posibles predicciones
    int8_t umbral = -128;       //Valor de control para prediccion
    int prediccion = -1;        //donde se almacena la prediccion

    for (int i = 0; i < output_size; i++)
    {
        //Obtiene el valor de una prediccion
        int8_t value = (*output).data.int8[i];
        ESP_LOGI(componente, "valor - %d", value);
        //La compara con el umbral y si es mayor toma el valor mayor
        if (value > umbral)
        {
            umbral = value; //Coloca como nuevo umbral el valor mayor
            prediccion = i; //Guarda el valor de la prediccion
        }
    }

    if (prediccion != -1)
    {
        *resultado = '0' + prediccion; 
        ESP_LOGI(componente, "Prediccion exitosa: %d", prediccion);
    }
    else
    {
        *resultado = '\0'; 
        ESP_LOGE(componente, "No suficientemente seguro para prediccion");
    }
}

/****************************************************************
 * Realiza la inferencia cargando el modelo, preprocesando la im-
 * agen y realizando la inferencia dejando el resultado de forma-
 * interna en el puntero output 
 *****************************************************************/
esp_err_t realiza_inferencia(uint8_t* input_buffer, char* resultado)
{
    const unsigned char *model_data = modelo_tflite; //Obtiene el modelo
    /* Si el puntero de es nulo le asigna espacio en la SPIRAM (Ram rapida) */
    if (tensor_arena == nullptr) {
        //Le asigna lognitud de kTensorArenaSize que va ser interpretrado como unsigned int 8
        tensor_arena = (uint8_t*) heap_caps_malloc(kTensorArenaSize, MALLOC_CAP_SPIRAM);
        if (tensor_arena == nullptr) {
            ESP_LOGE(componente, "Fallo al alojar tensor_arena en PSRAM");
            return ESP_ERR_NO_MEM;
        }
        ESP_LOGI(componente, "tensor_arena: %d KB en PSRAM", kTensorArenaSize / 1024);
    }
    int64_t start_time = esp_timer_get_time();  //Obtiene tiempo previo a cargar el modelo
    model = tflite::GetModel(model_data);       //Carga el modelo
    int64_t end_time = esp_timer_get_time();    //Obtiene tiempo al terminar de cargar el modelo
    //Evalua si la version del modelo
    if ((*model).version() != TFLITE_SCHEMA_VERSION)
    {
        /* Si falla retorna el error */
        ESP_LOGE(componente, "Model schema version mismatch");
        return ESP_ERR_NOT_SUPPORTED;
    }
    ESP_LOGI(componente, "Model load time: %.6f s", (end_time - start_time)/ 1000000.0);
    
    RegisterOps(micro_op_resolver); // Carga las operaciones validas para el modelo
    
    //Crea un interprete del modelo con las operaciones permitidas un area para los tensores y la longitud de esa area
    tflite::MicroInterpreter static_interpreter(model, micro_op_resolver, tensor_arena, kTensorArenaSize);
    interpreter = &static_interpreter; //Asigna la direccion de memoria a el puntero del interprete

    // Asignar tensores
    if ((*interpreter).AllocateTensors() != kTfLiteOk)
    {
        ESP_LOGE(componente, "Failed to allocate tensors.");
        return ESP_ERR_NO_MEM;
    }
    
    //Asigna el puntero al input del modelo input = input del modelo (imagen)
    input = (*interpreter).input(0);
    if (input == nullptr) {
        ESP_LOGE(componente, "input es nullptr — modelo incompatible o arena insuficiente");
        return ESP_ERR_NOT_SUPPORTED;
    }

    //Asigna el puntero a la salida del modelo output = salida del modelo (respuesta)
    output = (*interpreter).output(0);
    if (output == nullptr) {
        ESP_LOGE(componente, "output es nullptr");
        return ESP_ERR_INVALID_RESPONSE;
    }

    //Genera un arreglo de int8_t en spiram con ancho * alto * canales(R, G, B)
    int8_t *image_data = (int8_t*) heap_caps_malloc(ANCHO_IMAGEN * ALTO_IMAGEN * 3, MALLOC_CAP_SPIRAM);
    if (image_data == nullptr) {
        ESP_LOGE(componente, "Fallo al alojar image_data — PSRAM insuficiente");
        return ESP_ERR_NO_MEM;
    }

    //Realiza preproseso dependiendo si el modelo espera sin signo o con signo
    preprocesa_imagen(input_buffer, image_data);
    
    // Copiar datos preprocesados al tensor de entrada
    memcpy((*input).data.int8, image_data, sizeof(image_data));
    
    start_time = esp_timer_get_time();  //Obtiene tiempo antes de inferencia
    (*interpreter).Invoke();            //Realiza la inferencia
    end_time = esp_timer_get_time();    //Obtiene tiempo despues de inferencia
    ESP_LOGI(componente, "Inference time: %.6f s", (end_time - start_time)/ 1000000.0);
    // Manejar la salida del modelo
    printPredictedClass(resultado);
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