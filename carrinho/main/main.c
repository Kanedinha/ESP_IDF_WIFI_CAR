#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <esp_types.h>
#include <esp_rom_gpio.h>
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include <esp_wifi.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include <esp_http_server.h>

#include "iot_servo.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#define SERVO_CH0_PIN 2

#define EXAMPLE_ESP_WIFI_SSID      "RC_Car"
#define EXAMPLE_ESP_WIFI_PASS      "carrinho12"
#define EXAMPLE_ESP_WIFI_CHANNEL   1
#define EXAMPLE_MAX_STA_CONN       2

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "wifi soft AP";

void direction(float angle){
    iot_servo_write_angle(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, angle);
}

esp_err_t direction_handler(httpd_req_t *req){
    float angle = 0;  // como passo o parâmetro da página HTML para ca?
    direction(angle);
    // httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

httpd_uri_t direction_uri = {
    .uri      = "/direction",
    .method   = HTTP_GET,
    .handler  = direction_handler,
    .user_ctx = NULL
};

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

esp_err_t wifi_init_softap(void){
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .pmf_cfg = {
                    .required = false,
            },
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS, EXAMPLE_ESP_WIFI_CHANNEL);
    return ESP_OK;
}

esp_err_t web_server(){

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGI(TAG, "WEB Server failed !!!\n"); 
        return ESP_FAIL;
    }    
    ESP_LOGI(TAG, "WEB Server initialized sus !!!\n"); 

    httpd_register_uri_handler(server, &direction_uri);
    return ESP_OK;
}

void app_main() {
    esp_err_t ret;
    servo_config_t servo_cfg = {
        .max_angle = 50,
        .min_width_us = 500,
        .max_width_us = 2500,
        .freq = 50,
        .timer_number = LEDC_TIMER_0,
        .channels = {
            .servo_pin = {
                SERVO_CH0_PIN,
            },
            .ch = {
                LEDC_CHANNEL_0,
            },
        },
        .channel_number = 1,
    };

    iot_servo_init(LEDC_LOW_SPEED_MODE, &servo_cfg);

    float angle = 100.0f;

    ret = nvs_flash_init();
    if(ret != ESP_OK){
        ESP_LOGI(TAG, "NVS Flash init ERROR !!!\n");    
    }
    ESP_LOGI(TAG, "NVS Flash init\n");

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = wifi_init_softap();
    if(ret != ESP_OK){
        ESP_LOGI(TAG, "Wifi init ERROR !!!\n");    
    }
    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP\n");
    


    while(1){
        
    }

    iot_servo_deinit(LEDC_LOW_SPEED_MODE);
}
