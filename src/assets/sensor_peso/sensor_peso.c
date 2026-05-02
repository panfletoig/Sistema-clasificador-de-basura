#include "hx711.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define GPIO_D      GPIO_NUM_20
#define GPIO_SCK    GPIO_NUM_21

#define PESO_CONOCIDO_G     1000.0f
#define MUESTRAS_CALIBRAR   20        // más muestras al calibrar = más precisión
#define MUESTRAS_LEER       10

#define ESCALA              1.0f      // raw / gramo  ← se sobreescribe en RAM
#define OFFSET              0.0f      // raw en vacío ← se sobreescribe en RAM

static const char *TAG = "sensor_peso";

static hx711_t sensor = {
    .dout   = GPIO_D,
    .pd_sck = GPIO_SCK,
    .gain   = HX711_GAIN_A_128   // ganancia estándar para celdas de carga
};

static float escala  = ESCALA;
static float offset  = OFFSET;
static bool  listo   = false;   // flag: init ya fue llamado

static esp_err_t leer_raw(int32_t *out_raw)
{
    esp_err_t err = hx711_wait(&sensor, 500);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Sensor no responde: %s", esp_err_to_name(err));
        return err;
    }
    return hx711_read_average(&sensor, MUESTRAS_LEER, out_raw);
}

esp_err_t sensor_peso_init(void)
{
    if (listo) {
        ESP_LOGW(TAG, "sensor_peso_init ya fue llamado");
        return ESP_OK;
    }

    esp_err_t err = hx711_init(&sensor);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "hx711_init falló: %s", esp_err_to_name(err));
        return err;
    }

    // Espera que el sensor salga del estado de power-on
    vTaskDelay(pdMS_TO_TICKS(400));

    listo = true;
    ESP_LOGI(TAG, "HX711 inicializado OK");
    return ESP_OK;
}

esp_err_t sensor_calibrar_offset(void)
{
    if (!listo) return ESP_ERR_INVALID_STATE;

    int32_t raw = 0;
    // Usamos más muestras para mayor precisión en la calibración
    esp_err_t err = hx711_wait(&sensor, 500);
    if (err != ESP_OK) return err;
    err = hx711_read_average(&sensor, MUESTRAS_CALIBRAR, &raw);
    if (err != ESP_OK) return err;

    offset = (float)raw;
    ESP_LOGI(TAG, "─── OFFSET capturado: %.0f ───", offset);
    ESP_LOGI(TAG, "Pon el peso conocido (%.0f g) y llama sensor_calibrar_escala()", PESO_CONOCIDO_G);
    return ESP_OK;
}

esp_err_t sensor_calibrar_escala(void)
{
    if (!listo) return ESP_ERR_INVALID_STATE;

    int32_t raw = 0;
    esp_err_t err = hx711_wait(&sensor, 500);
    if (err != ESP_OK) return err;
    err = hx711_read_average(&sensor, MUESTRAS_CALIBRAR, &raw);
    if (err != ESP_OK) return err;

    float diferencia = (float)raw - offset;
    if (diferencia == 0.0f) {
        ESP_LOGE(TAG, "diferencia es 0 — verifica conexiones o peso");
        return ESP_ERR_INVALID_RESPONSE;
    }

    escala = diferencia / PESO_CONOCIDO_G;

    ESP_LOGI(TAG, "─────────────────────────────────────────");
    ESP_LOGI(TAG, "  #define OFFSET   %.0f", offset);
    ESP_LOGI(TAG, "  #define ESCALA   %.4f", escala);
    ESP_LOGI(TAG, "  Copia estos valores y flashea de nuevo");
    ESP_LOGI(TAG, "─────────────────────────────────────────");
    return ESP_OK;
}

// ─── Lectura de peso ──────────────────────────────────────────────────────────

float sensor_peso_leer(void)
{
    if (!listo) {
        ESP_LOGE(TAG, "Llama sensor_peso_init() primero");
        return -1.0f;
    }

    int32_t raw = 0;
    esp_err_t err = leer_raw(&raw);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error al leer: %s", esp_err_to_name(err));
        return -1.0f;
    }

    float peso_g = ((float)raw - offset) / escala;

    if (peso_g < -500.0f || peso_g > 20000.0f) {
        ESP_LOGW(TAG, "Fuera de rango: %.2f g (raw=%ld)", peso_g, raw);
        return -1.0f;
    }

    ESP_LOGI(TAG, "Raw: %ld | Peso: %.2f g (%.3f kg)", raw, peso_g, peso_g / 1000.0f);
    return peso_g;
}