/*
//Librerias ESP32
#include "freertos/event_groups.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"  

static EventGroupHandle_t http_client_event;
static const char* component = "http_client";

static void http_client_event_handler(void *args, esp_event_base_t base, int32_t event_id, void *event_data){
    if(event_id == 1){
        return
    }
}

void http_post(const char *url, const char *json_body) {
    char response[512] = {0};

    esp_http_client_config_t config = {
        .url            = url,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach  = esp_crt_bundle_attach, // ← valida con CAs del sistema
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    // Configurar método y headers
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_body, strlen(json_body));

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(component, "Status: %d", status);

        // Leer respuesta
        int read = esp_http_client_read_response(client, response, sizeof(response) - 1);
        if (read > 0) {
            ESP_LOGI(component, "Respuesta: %s", response);
        }
    } else {
        ESP_LOGE(component, "Error: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}
*/