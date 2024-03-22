/* MQTT (over TCP) Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "driver/gpio.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "wifi_helper.h"
#include "mqtt_helper.h"
#include "pin_definitions.h"

static const char *TAG = "Panavista Capstone";

extern "C" {
	void app_main(void);
    esp_err_t esp_netif_init(void);
    esp_err_t esp_event_loop_create_default(void);
    esp_err_t wifi_helper_connect(void);
    void mqtt_helper_start(void);
}

extern void task_initI2C(void*);
extern void task_display(void*);
extern void task_display2(void*);

void init_gpio(void)
{
    gpio_reset_pin(LED_GREEN0);
    gpio_reset_pin(LED_GREEN1);
    gpio_reset_pin(LED_GREEN2);
    gpio_reset_pin(LED_RED0);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(LED_GREEN0, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_GREEN1, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_GREEN2, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_RED0, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_RED0, 0);
    gpio_set_level(LED_GREEN0, 0);
    gpio_set_level(LED_GREEN1, 0);
    gpio_set_level(LED_GREEN2, 0);
}

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("outbox", ESP_LOG_VERBOSE);

    /* Initialize NVS.
     * NVS needs to instantiated to store WiFi configurations
     */
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
        /* Erase NVS and try again */
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

    /* Init the TCP IP stack 
     * https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_netif.html
     */
    ESP_ERROR_CHECK(esp_netif_init());

    /* Create a default event loop
     * https://docs.espressif.com/projects/esp-idf/en/v5.2.1/esp32/api-reference/system/esp_event.html
     */
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    init_gpio();

    

    wifi_helper_connect();

    mqtt_helper_start();
    vTaskDelay(1000/portTICK_PERIOD_MS);
    xTaskCreatePinnedToCore(&task_initI2C, "mpu_task", 2048, NULL, 5, NULL, 1);
    vTaskDelay(500/portTICK_PERIOD_MS);
    xTaskCreatePinnedToCore(&task_display, "disp_task", 8192, NULL, 5, NULL, 1);

}
