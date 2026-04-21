#include "driver/sdmmc_host.h"
#include "driver/gpio.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "esp_log.h"

#define SD_CMD  38
#define SD_CLK  39
#define SD_DATA 40

static const char* componente = "MICROSD";

void mount_sd(void) {
    vTaskDelay(pdMS_TO_TICKS(500));

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_DEFAULT; // 20 MHz

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.clk   = SD_CLK;
    slot_config.cmd   = SD_CMD;
    slot_config.d0    = SD_DATA;
    slot_config.width = 1; // 1-bit mode (solo DAT0)

    // Pull-ups internos
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files              = 5,
        .allocation_unit_size   = 16 * 1024,
    };

    sdmmc_card_t *card;
    esp_err_t err = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);
    if (err != ESP_OK) {
        ESP_LOGE(componente, "Error montando SD: %s", esp_err_to_name(err));
        return;
    }

    sdmmc_card_print_info(stdout, card);
    ESP_LOGI(componente, "SD montada correctamente");
}

void save_as_bmp(const char *filename, uint8_t *rgb, int width, int height)
{
    return;

    ESP_LOGI(componente, "Iniciando guardado BMP");

    if (rgb == NULL) {
        ESP_LOGE(componente, "Buffer RGB es NULL");
        return;
    }

    FILE *f = fopen(filename, "wb");
    if (!f) {
        ESP_LOGE(componente, "No se pudo abrir el archivo");
        return;
    }

    int filesize = 54 + 3 * width * height;

    ESP_LOGI(componente, "Resolucion: %dx%d", width, height);
    ESP_LOGI(componente, "Tamano archivo: %d bytes", filesize);

    uint8_t fileHeader[14] = {
        'B','M',
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

    ESP_LOGI(componente, "Escribiendo pixeles...");

    // BMP guarda en BGR y de abajo hacia arriba
    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            int i = (y * width + x) * 3;

            uint8_t r = rgb[i];
            uint8_t g = rgb[i + 1];
            uint8_t b = rgb[i + 2];

            uint8_t pixel[3] = {b, g, r};
            fwrite(pixel, 1, 3, f);
        }
    }

    fclose(f);

    ESP_LOGI(componente, "Imagen guardada correctamente en: %s", filename);
}