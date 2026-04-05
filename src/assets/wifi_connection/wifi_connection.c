#ifndef WIFI_CONNECTION_C
#define WIFI_CONNECTION_C
//Librerias de C
#include <string.h>
//Librerias de EXPRESSIF
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_log.h"

//Variables Persistentes en RTC - WIFI (Optimiza tiempo de conexion a internet)
RTC_DATA_ATTR uint8_t  rtc_bssid[6], rtc_channel;
RTC_DATA_ATTR uint32_t rtc_ip, rtc_gw, rtc_mask;
RTC_DATA_ATTR bool     rtc_wifi_saved = false;

#define wifi_id "TIGO-4488"
#define wifi_pwd "2NJ555301639"

static const char component[] = "WIFI_CONNECTION";  

void save_wifi_info() {
    wifi_ap_record_t ap;
    esp_wifi_sta_get_ap_info(&ap);

    memcpy(rtc_bssid, ap.bssid, 6);
    rtc_channel = ap.primary;

    // Guardar IP estática
    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_get_ip_info(netif, &ip_info);

    rtc_ip   = ip_info.ip.addr;
    rtc_gw   = ip_info.gw.addr;
    rtc_mask = ip_info.netmask.addr;
    ESP_LOGI(component, "IP: " IPSTR, IP2STR(&ip_info.ip)); 

    rtc_wifi_saved = true;
}

void fast_wifi_connect() {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_t *netif = esp_netif_create_default_wifi_sta();

    // Asignar IP estática (evita DHCP)
    esp_netif_dhcpc_stop(netif);
    esp_netif_ip_info_t ip_info = {
        .ip.addr      = rtc_ip,
        .gw.addr      = rtc_gw,
        .netmask.addr = rtc_mask,
    };
    esp_netif_set_ip_info(netif, &ip_info);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);

    // Conectar directo al canal y BSSID conocidos
    wifi_config_t wifi_cfg = {
        .sta = {
            .ssid     = wifi_id,
            .password = wifi_pwd,
            .channel  = rtc_channel,
            .bssid_set = true,
        }
    };
    memcpy(wifi_cfg.sta.bssid, rtc_bssid, 6);

    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    esp_wifi_start();
    esp_wifi_connect();
    save_wifi_info();
}
#endif