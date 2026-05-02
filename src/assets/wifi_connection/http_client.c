//Librerias ESP32
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"  
#include "esp_log.h"

static const char* componente = "http_client";

void http_post(const char *url, const char *json_body) {
    char *response = malloc(512);
    if (!response) {
        ESP_LOGE(componente, "No hay memoria");
        return;
    }
    esp_http_client_config_t config = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    // Configurar método y headers
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_body, strlen(json_body));

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(componente, "Status: %d", status);

        // Leer respuesta
        int read = esp_http_client_read_response(client, response, sizeof(response) - 1);
        if (read > 0) {
            ESP_LOGI(componente, "Respuesta: %s", response);
        }
    } else {
        ESP_LOGE(componente, "Error: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    free(response);
}