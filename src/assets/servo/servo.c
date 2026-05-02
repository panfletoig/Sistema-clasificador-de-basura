#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_err.h"

// ─── Pines ────────────────────────────────────────────────────────────────────
#define PIN_SERVO_X         GPIO_NUM_47
#define PIN_SERVO_Z         GPIO_NUM_48

// ─── PWM Común ────────────────────────────────────────────────────────────────
#define SERVO_PWM_MODE      LEDC_LOW_SPEED_MODE
#define SERVO_PWM_TIMER     LEDC_TIMER_0
#define SERVO_PWM_FREQ_HZ   50
#define SERVO_PWM_RES       LEDC_TIMER_14_BIT
#define SERVO_PERIOD_US     20000

// ─── Canales (uno por servo) ──────────────────────────────────────────────────
#define SERVO_X_CHANNEL     LEDC_CHANNEL_0
#define SERVO_Z_CHANNEL     LEDC_CHANNEL_1

// ─── Rangos de posición (en valor ADC 0–4095) ─────────────────────────────────
#define SERVO_X_MIN         100
#define SERVO_X_MAX         4095

#define SERVO_Z_MIN         500     // ← ajusta según tu servo/mecanismo
#define SERVO_Z_MAX         3500

// ─── Pulso PWM en microsegundos ───────────────────────────────────────────────
#define SERVO_MIN_US        500
#define SERVO_MAX_US        2500

static const char *TAG = "servo";
static bool initialized = false;

static uint32_t pulse_us_to_duty(uint32_t pulse_us)
{
    uint32_t max_duty = (1 << SERVO_PWM_RES) - 1;
    return (pulse_us * max_duty) / SERVO_PERIOD_US;
}

static uint32_t adc_to_pulse_us(int adc_val, int adc_min, int adc_max)
{
    // Clamp
    if (adc_val < adc_min) adc_val = adc_min;
    if (adc_val > adc_max) adc_val = adc_max;

    // Mapeo lineal ADC → microsegundos de pulso
    return SERVO_MIN_US +
           ((uint32_t)(adc_val - adc_min) * (SERVO_MAX_US - SERVO_MIN_US)) /
           (uint32_t)(adc_max - adc_min);
}

static esp_err_t set_duty(ledc_channel_t canal, uint32_t duty)
{
    esp_err_t err;

    err = ledc_set_duty(SERVO_PWM_MODE, canal, duty);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_set_duty: %s", esp_err_to_name(err));
        return err;
    }
    err = ledc_update_duty(SERVO_PWM_MODE, canal);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_update_duty: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t servo_init(void)
{
    if (initialized) {
        ESP_LOGW(TAG, "servo_init llamado más de una vez, ignorando");
        return ESP_OK;
    }

    esp_err_t err;

    // Timer compartido por ambos canales
    ledc_timer_config_t timer_cfg = {
        .speed_mode      = SERVO_PWM_MODE,
        .duty_resolution = SERVO_PWM_RES,
        .timer_num       = SERVO_PWM_TIMER,
        .freq_hz         = SERVO_PWM_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    err = ledc_timer_config(&timer_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_timer_config: %s", esp_err_to_name(err));
        return err;
    }

    // Canal Servo X (MG996R — GPIO 47)
    ledc_channel_config_t ch_x = {
        .gpio_num   = PIN_SERVO_X,
        .speed_mode = SERVO_PWM_MODE,
        .channel    = SERVO_X_CHANNEL,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = SERVO_PWM_TIMER,
        .duty       = pulse_us_to_duty(0),   // posición inicial
        .hpoint     = 0,
        .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
    };
    err = ledc_channel_config(&ch_x);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_channel_config X: %s", esp_err_to_name(err));
        return err;
    }

    // Canal Servo Z (MG995 — GPIO 48)
    ledc_channel_config_t ch_z = {
        .gpio_num   = PIN_SERVO_Z,
        .speed_mode = SERVO_PWM_MODE,
        .channel    = SERVO_Z_CHANNEL,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = SERVO_PWM_TIMER,
        .duty       = pulse_us_to_duty(0),
        .hpoint     = 0,
        .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
    };
    err = ledc_channel_config(&ch_z);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_channel_config Z: %s", esp_err_to_name(err));
        return err;
    }

    initialized = true;
    ESP_LOGI(TAG, "Servos inicializados correctamente");
    return ESP_OK;
}

esp_err_t servo_x_mover(int adc_val)
{
    if (!initialized) {
        ESP_LOGE(TAG, "servo_init() no fue llamado");
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t pulse_us = adc_to_pulse_us(adc_val, SERVO_X_MIN, SERVO_X_MAX);
    uint32_t duty     = pulse_us_to_duty(pulse_us);

    ESP_LOGI(TAG, "Servo X → adc=%d  pulse=%luµs  duty=%lu", adc_val, pulse_us, duty);

    esp_err_t err = set_duty(SERVO_X_CHANNEL, duty);
    vTaskDelay(pdMS_TO_TICKS(1000));         // tiempo de movimiento
    ledc_stop(SERVO_PWM_MODE, SERVO_X_CHANNEL, 0);
    return err;
}

esp_err_t servo_z_mover(int adc_val)
{
    if (!initialized) {
        ESP_LOGE(TAG, "servo_init() no fue llamado");
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t pulse_us = adc_to_pulse_us(adc_val, SERVO_Z_MIN, SERVO_Z_MAX);
    uint32_t duty     = pulse_us_to_duty(pulse_us);

    ESP_LOGI(TAG, "Servo Z → adc=%d  pulse=%luµs  duty=%lu", adc_val, pulse_us, duty);

    esp_err_t err = set_duty(SERVO_Z_CHANNEL, duty);
    vTaskDelay(pdMS_TO_TICKS(1000));
    ledc_stop(SERVO_PWM_MODE, SERVO_Z_CHANNEL, 0);
    return err;
}