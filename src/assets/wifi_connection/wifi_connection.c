//Librerías de ESPRESSIF    
#include "freertos/FreeRTOS.h"      //Nucleo del sistema operativo FreeRTOS (requerido por todas las demás librerías de FreeRTOS)
#include "freertos/task.h"          //Creación y manejo de tareas en FreeRTOS
#include "freertos/event_groups.h"  //Grupos de eventos para sincronización entre tareas
#include "esp_err.h"                //Códigos de error y macros de manejo de errores (esp_err_t, ESP_ERROR_CHECK)
#include "esp_wifi.h"               //Configuración y control del módulo Wi-Fi
#include "esp_netif.h"              //Abstracción de interfaz de red (TCP/IP stack)
#include "esp_log.h"                //Sistema de logging con niveles (ERROR, WARN, INFO, DEBUG)
#include "lwip/dns.h"


#include "assets/wifi_connection/http_client.h"

//Credenciales de RED
//#define wifi_id "TIGO-4488"         //Wifi_net_identifier
//#define wifi_pwd "2NJ555301639"     //Wifi_pass
#define wifi_id "Carlosint"         //Wifi_net_identifier
#define wifi_pwd "semillero"     //Wifi_pass

//Bits usados para señalizar el resultado
#define WIFI_CONNECTED_BIT BIT0     //Event bit por si se conecta
#define WIFI_FAIL_BIT      BIT1     //Event bit por si falla

// Variables Privadas
static const char* componente = "wifi_connection";   //Tag para el LOG
static EventGroupHandle_t wifi_events;              //Manejador de eventos

//Variables Persistentes en RTC - WIFI (Optimiza tiempo de conexion a internet)
static RTC_DATA_ATTR uint8_t   rtc_bssid[6];           //MAC address del router
static RTC_DATA_ATTR uint8_t   rtc_channel;            //Canal Wifi (Frecuencia del router)
static RTC_DATA_ATTR uint32_t  rtc_ip;                 //Direccion IP del dispositivo
static RTC_DATA_ATTR uint32_t  rtc_gw;                 //Gateway (IP del router)
static RTC_DATA_ATTR uint32_t  rtc_mask;               //Mascara de subred
static RTC_DATA_ATTR bool      rtc_wifi_saved = false; //Indica si esta guardada la conexion

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t event_id, void *event_data) {
    if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        /* Si ya hay una ip disponible entonces desbloquea el evento guarda el evento */ 
        xEventGroupSetBits(wifi_events, WIFI_CONNECTED_BIT);
    }
}

//Guarda los parametos de conexion
static void guardar_conexion() {
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"); //Provee configuracion del WIFI
    esp_netif_ip_info_t ip_info;    //Estructura con la informacion de la IP
    wifi_ap_record_t ap;            //Estructura con informacion de conexion
    esp_netif_get_ip_info(netif, &ip_info); //Obtiene la informacion de la IP
    esp_wifi_sta_get_ap_info(&ap);  //Obtiene la informacion de conexion

    memcpy(rtc_bssid, ap.bssid, 6);     //Copia la MAC ADDRESS del router a la variable RTC
    rtc_channel = ap.primary;           //Copia el canal a la variable RTC
    rtc_ip   = ip_info.ip.addr;         //Guarda la IP en variable RTC
    rtc_gw   = ip_info.gw.addr;         //Guarda el GATEWAY en variable RTC
    rtc_mask = ip_info.netmask.addr;    //Guarda la mascara en variable RTC
    ESP_LOGI(componente, "IP: " IPSTR, IP2STR(&ip_info.ip)); //Imprime la ip en el LOG 

    rtc_wifi_saved = true; //Coloca como guardado el wifi en variable RTC
}

void set_dns_manual() {
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == NULL) {
        ESP_LOGE("DNS", "netif NULL — llama set_dns_manual() después de esp_wifi_start()");
        return;
    }

    esp_netif_dns_info_t dns = {
        .ip.u_addr.ip4.addr = ipaddr_addr("8.8.8.8"),
        .ip.type            = IPADDR_TYPE_V4,
    };
    esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns);
    ESP_LOGI("DNS", "DNS configurado: " IPSTR, IP2STR(&dns.ip.u_addr.ip4));
}

static void fast_wifi_connect() {
    wifi_events = xEventGroupCreate();
    esp_event_loop_create_default();
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,    &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT,   IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);

    esp_netif_init();
    esp_netif_t *netif = esp_netif_create_default_wifi_sta();
    esp_netif_dhcpc_stop(netif);

    esp_netif_ip_info_t ip_info = {
        .ip.addr      = rtc_ip,
        .gw.addr      = rtc_gw,
        .netmask.addr = rtc_mask,
    };
    esp_netif_set_ip_info(netif, &ip_info);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);  
    esp_wifi_set_mode(WIFI_MODE_STA);

    wifi_config_t wifi_cfg = {
        .sta = {
            .ssid      = wifi_id,
            .password  = wifi_pwd,
            .channel   = rtc_channel,
            .bssid_set = true,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e        = WPA3_SAE_PWE_HUNT_AND_PECK,
        }
    };
    memcpy(wifi_cfg.sta.bssid, rtc_bssid, 6);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    esp_wifi_start();
    esp_wifi_set_ps(WIFI_PS_NONE); 
    set_dns_manual();              
    esp_wifi_connect();

    EventBits_t bits = xEventGroupWaitBits(
        wifi_events, WIFI_CONNECTED_BIT,
        pdFALSE, pdFALSE, pdMS_TO_TICKS(1000)
    );

    if (bits & WIFI_CONNECTED_BIT) {
        char* json = "{\"categoria\":\"otros\",\"peso_g\":20,...}";
        http_post("https://recibirresiduo-epnwwyvrcq-uc.a.run.app/recibirResiduo", json);
    } else {
        ESP_LOGE(componente, "No se pudo conectar");
    }
}

static void normal_wifi_connect(){
    wifi_events = xEventGroupCreate();      //Crea el event group
    esp_event_loop_create_default();        //Inicia el loop de Eventos
    //Registrar eventos para desconexion o evento cualquiera y obtencion de IP
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,  &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT,   IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);
    
    esp_netif_init();                       //Inicia la capa TCP/IP
    esp_netif_create_default_wifi_sta();    //Inicia la interfaz wifi STA
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();    //Estructura de configuracion DEFAULT
    esp_wifi_init(&cfg);                                    //Inicia el driver con la configuracion default
    esp_wifi_set_storage(WIFI_STORAGE_RAM);                 //Almacena en ram
    esp_wifi_set_mode(WIFI_MODE_STA);                       //Modo cliente (estacion)
    
    //Estructura con la configuracion del wifi (Wifi_ID, Wifi_PWD, Modo de autentificacion, Certificados)
    wifi_config_t wifi_cfg = {
        .sta = {
            .ssid     = wifi_id,
            .password = wifi_pwd,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,           // Seguridad mínima aceptada
            .sae_pwe_h2e        = WPA3_SAE_PWE_HUNT_AND_PECK,   // Compatibilidad WPA3
        }
    };
    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);    //Coloca la configuracion del WIFI
    esp_wifi_start();                               //Enciende el driver
    esp_wifi_set_ps(WIFI_PS_NONE);                         
    set_dns_manual();
    esp_wifi_connect();                             //Establece la conexion
    
    //Esperar 10 segundos a que la conexion se establezca y setea los bits 
    EventBits_t bits = xEventGroupWaitBits(wifi_events, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(1000));
    if (bits & WIFI_CONNECTED_BIT) {
        guardar_conexion(); //Guarda la conexion si se logro establecer
    } else {
        ESP_LOGE(componente, "No se pudo conectar");
    }
}

//Selecciona que metodo de conexion usar
void establecer_conexion(){
    if (rtc_wifi_saved){
        //Si se guardaron los parametros de conexion se usa el metodo rapido
        ESP_LOGI(componente, "Usando Conexion Rapida");
        fast_wifi_connect();
    }
    else{
        //Si es primera conexion usa el metodo lento
        ESP_LOGI(componente, "Establecer Primera Conexion");
        normal_wifi_connect();
    }
}