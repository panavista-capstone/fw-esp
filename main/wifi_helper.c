#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "esp_types.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "driver/gpio.h"

#include "wifi_helper.h"
#include "pin_definitions.h"


/* FreeRTOS event group to signal when we are connected */
static EventGroupHandle_t s_wifi_event_group;

static const char *TAG = "WiFi Helper";
static int retry_num = 0;
static const int WIFI_CONNECTED_BIT = BIT0;
static const int WIFI_FAIL_BIT = BIT1;
#define WIFI_SSID "BobaHome2.4"
#define WIFI_PASS ""

static const int WIFI_CONNECT_MAX_RETRY = 5;


// Event Handler for the WiFi Stack
// https://docs.espressif.com/projects/esp-idf/en/v5.2.1/esp32s3/api-guides/wifi.html#wifi-programming-model
static void event_handler_wifi(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base != WIFI_EVENT) {
        ESP_LOGE(TAG, "WiFi event handler not handling WiFi event");
        abort();
    }

    switch (event_id)
    {
        case WIFI_EVENT_STA_START:
            esp_wifi_connect();
            ESP_LOGI(TAG, "WiFi event handler start connect");
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
            if (retry_num < WIFI_CONNECT_MAX_RETRY) {
                esp_wifi_connect();
                retry_num++;
                ESP_LOGI(TAG, "WiFi event handler connection retry #%d", retry_num);

            } else {
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                ESP_LOGE(TAG, "WiFi event handler connection failed #%d", retry_num);
            }
            break;

        default:
            break;
    }
}

// Event Handler for the lwIP Stack
// https://docs.espressif.com/projects/esp-idf/en/v5.2.1/esp32s3/api-guides/wifi.html#wifi-programming-model
static void event_handler_ip(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base != IP_EVENT) {
        ESP_LOGE(TAG, "IP event handler not handling IP event");
        abort();
    }

    switch (event_id)
    {
        case IP_EVENT_STA_GOT_IP:
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            ESP_LOGI(TAG, "IP event handler got IP:" IPSTR, IP2STR(&event->ip_info.ip));
            retry_num = 0;
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            break;

        default:
            break;
    }
}

/**
  * \brief  Connect to the Wi-Fi access point.
  * \note	The SSID and pasword are in the 'station.h' file.
  */
static void start_station(void)
{
    s_wifi_event_group = xEventGroupCreate();

    // https://docs.espressif.com/projects/esp-idf/en/v5.2.1/esp32s3/api-reference/network/esp_netif.html#wi-fi-default-initialization
    esp_netif_t * netif_inst = esp_netif_create_default_wifi_sta();

    // Always use WIFI_INIT_CONFIG_DEFAULT macro to initialize the configuration to default values,
    // this can guarantee all the fields get correct value when more fields are added into
    // wifi_init_config_t in future release
    // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_wifi.html
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler_wifi,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler_ip,
                                                        NULL,
                                                        &instance_got_ip));

    /* Set the Wi-Fi access point parameters. */
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	        .threshold.authmode = WIFI_AUTH_WPA2_PSK,

            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    // https://docs.espressif.com/projects/esp-idf/en/v5.2.1/esp32s3/api-reference/network/esp_wifi.html
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    // Config stored in NVS
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi init finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdTRUE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits just before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {

        ESP_LOGI(TAG, "Connected to AP SSID:%s password:%s",
                 WIFI_SSID, WIFI_PASS);
        gpio_set_level(LED_GREEN2, 1);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to AP SSID:%s, password:%s",
                 WIFI_SSID, WIFI_PASS);
        gpio_set_level(LED_RED0, 1);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    // TODO: Do not delete this
    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}

esp_err_t wifi_helper_connect(void)
{
    ESP_LOGI(TAG, "WiFi connect start");
    start_station();
    return ESP_OK;
}