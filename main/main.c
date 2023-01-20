#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>

#include <esp_types.h>
#include <esp_rom_gpio.h>
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include <esp_wifi.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_http_server.h"

#include "bmp280.h"

#include "driver/i2c.h"
#include "iot_servo.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#define SERVO_CH0_PIN 2

#define EXAMPLE_ESP_WIFI_SSID      "Projetos"
#define EXAMPLE_ESP_WIFI_PASS      "ftn22182"
#define EXAMPLE_ESP_MAXIMUM_RETRY  10

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#define I2C_MASTER_FREQ_HZ 40000
#define I2C_MASTER_SDA_IO 21
#define I2C_MASTER_SCL_IO 22
#define BMP280_ADDR 0x76

#define I2C_MASTER_TX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define WRITE_BIT I2C_MASTER_WRITE  /*!< I2C master write */
#define READ_BIT I2C_MASTER_READ    /*!< I2C master read */
#define ACK_CHECK_EN 0x1            /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0           /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x0                 /*!< I2C ack value */
#define NACK_VAL 0x1                /*!< I2C nack value */

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)
#define SCRATCH_BUFSIZE  8192

struct file_server_data {
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
};

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
static const char *TAG = "wifi station";

float angle = 0;
int8_t speed = 0; //velocidade varia de -100% até 100%

#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filename)
{
    if (IS_FILE_EXT(filename, ".pdf"))
    {
        ESP_LOGI(TAG, "file is a .pdf");
        return httpd_resp_set_type(req, "application/pdf");
    }
    else if (IS_FILE_EXT(filename, ".html"))
    {
        ESP_LOGI(TAG, "file is a .html");
        return httpd_resp_set_type(req, "text/html");
    }
    else if (IS_FILE_EXT(filename, ".jpeg"))
    {
        ESP_LOGI(TAG, "file is a .jpeg");
        return httpd_resp_set_type(req, "image/jpeg");
    }
    else if (IS_FILE_EXT(filename, ".ico"))
    {
        ESP_LOGI(TAG, "file is a .ico");
        return httpd_resp_set_type(req, "image/x-icon");
    }
    else if (IS_FILE_EXT(filename, ".js"))
    {
        ESP_LOGI(TAG, "file is a .js");
        return httpd_resp_set_type(req, "script");
    }
    else if (IS_FILE_EXT(filename, ".css"))
    {
        ESP_LOGI(TAG, "file is a .css");
        return httpd_resp_set_type(req, "text/css");
    }
    else if (IS_FILE_EXT(filename, ".gif"))
    {
        ESP_LOGI(TAG, "file is a .gif");
        return httpd_resp_set_type(req, "image/gif");
    }
    else if (IS_FILE_EXT(filename, ".png"))
    {
        ESP_LOGI(TAG, "file is a .png");
        return httpd_resp_set_type(req, "image/png");
    }
    /* This is a limited set only */
    /* For any other type always set as plain text */
    return httpd_resp_set_type(req, "text/plain");
}

void direction(float angle){
    iot_servo_write_angle(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, angle);
}

static const char* get_path_from_uri(char *dest, const char *base_path, const char *uri, size_t destsize)
{
    const size_t base_pathlen = strlen(base_path);
    size_t pathlen = strlen(uri);

    const char *quest = strchr(uri, '?');
    if (quest) {
        pathlen = MIN(pathlen, quest - uri);
    }
    const char *hash = strchr(uri, '#');
    if (hash) {
        pathlen = MIN(pathlen, hash - uri);
    }

    if (base_pathlen + pathlen + 1 > destsize) {
        /* Full path string won't fit into destination buffer */
        return NULL;
    }

    /* Construct full path (base + path) */
    strcpy(dest, base_path);
    strlcpy(dest + base_pathlen, uri, pathlen + 1);

    /* Return pointer to path, skipping the base */
    return dest + base_pathlen;
}

static esp_err_t root_handler(httpd_req_t *req){

    extern const uint8_t index_html_start[] asm("_binary_page_html_start"); // uint8_t
    extern const uint8_t index_html_end[] asm("_binary_page_html_end");     // uint8_t

    httpd_resp_set_type(req, "page.html");
    httpd_resp_send(req, (char *)index_html_start, index_html_end - index_html_start - 1);
    
    return ESP_OK;
}

static esp_err_t root_assets_handler(httpd_req_t *req){
    
    if(strcasecmp((char*)req->uri, "/assets/Fortron.png") == 0){
        extern const uint8_t Fortron_start[] asm("_binary_Fortron_png_start"); // uint8_t
        extern const uint8_t Fortron_end[] asm("_binary_Fortron_png_end");     // uint8_t
        set_content_type_from_file(req, (char*)req->uri);
        httpd_resp_send(req, (char *)Fortron_start, Fortron_end - Fortron_start - 1);
        ESP_LOGI(TAG, "%d req: %s", __LINE__, (char*)req->uri);
    }
    else if(strcasecmp((char*)req->uri, "/assets/trator.gif") == 0){
        extern const uint8_t trator_start[] asm("_binary_trator_gif_start"); // uint8_t
        extern const uint8_t trator_end[] asm("_binary_trator_gif_end");     // uint8_t
        set_content_type_from_file(req, (char*)req->uri);
        httpd_resp_send(req, (char *)trator_start, trator_end - trator_start - 1);
        ESP_LOGI(TAG, "%d req: %s", __LINE__, (char*)req->uri);
    }
    else if(strcasecmp((char*)req->uri, "/assets/style.css") == 0){
        extern const uint8_t style_start[] asm("_binary_style_css_start"); // uint8_t
        extern const uint8_t style_end[] asm("_binary_style_css_end");     // uint8_t
        set_content_type_from_file(req, (char*)req->uri);
        httpd_resp_send(req, (char *)style_start, style_end - style_start - 1);
        ESP_LOGI(TAG, "%d req: %s", __LINE__, (char*)req->uri);
    }
    else if(strcasecmp((char*)req->uri, "/assets/code.js") == 0){
        extern const uint8_t code_start[] asm("_binary_code_js_start"); // uint8_t
        extern const uint8_t code_end[] asm("_binary_code_js_end");     // uint8_t
        set_content_type_from_file(req, (char*)req->uri);
        httpd_resp_send(req, (char *)code_start, code_end - code_start - 1);
        ESP_LOGI(TAG, "%d req: %s", __LINE__, (char*)req->uri);
    }
    else if(strcasecmp((char*)req->uri, "/assets/F.ico") == 0){
        extern const uint8_t F_start[] asm("_binary_F_ico_start"); // uint8_t
        extern const uint8_t F_end[] asm("_binary_F_ico_end");     // uint8_t
        set_content_type_from_file(req, (char*)req->uri);
        httpd_resp_send(req, (char *)F_start, F_end - F_start - 1);
        ESP_LOGI(TAG, "%d req: %s", __LINE__, (char*)req->uri);
    }
    return ESP_OK;
}

static esp_err_t direction_handler(httpd_req_t *req){
    if(angle == 0)
        angle = 89;
    else
        angle = 0;
    direction(angle);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

httpd_uri_t direction_uri = {
    .uri      = "/direction",
    .method   = HTTP_POST,
    .handler  = direction_handler,
    .user_ctx = NULL
};

httpd_uri_t root_uri = {
    .uri      = "/",
    .method   = HTTP_GET,
    .handler  = root_handler,
    .user_ctx = NULL
};

httpd_uri_t root_assets_uri = {
    .uri      = "/assets/*",
    .method   = HTTP_GET,
    .handler  = root_assets_handler,
    .user_ctx = NULL
};

esp_err_t wifi_init(void){
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
	     .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
    
    return ESP_OK;
}

esp_err_t web_server(){

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGI(TAG, "WEB Server failed !!!\n"); 
        return ESP_FAIL;
    }    
    ESP_LOGI(TAG, "WEB Server initialized sus !!!\n"); 

    httpd_register_uri_handler(server, &root_assets_uri);
    httpd_register_uri_handler(server, &direction_uri);
    httpd_register_uri_handler(server, &root_uri);
    return ESP_OK;
}

void peripherical_init(){
    servo_config_t servo_cfg = {
        .max_angle = 180,
        .min_width_us = 500,
        .max_width_us = 2500,
        .freq = 180,
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
    direction(0);

    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0));
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,         // select GPIO specific to your project
        .scl_io_num = I2C_MASTER_SCL_IO,         // select GPIO specific to your project
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,  // select frequency specific to your project
        .clk_flags = 0,                          // you can use I2C_SCLK_SRC_FLAG_* flags to choose i2c source clock here
    };
    i2c_param_config(I2C_NUM_0, &conf);
  

}

void i2c_write(uint8_t dev_addr, uint8_t reg_addr, uint8_t *reg_data, uint8_t cnt)
{
	uint8_t iError = 0;

	esp_err_t espRc;
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();

	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);

	i2c_master_write_byte(cmd, reg_addr, true);
	i2c_master_write(cmd, reg_data, cnt, true);
	i2c_master_stop(cmd);

	espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10 / portTICK_PERIOD_MS);
	
	i2c_cmd_link_delete(cmd);
}

uint8_t i2c_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *reg_data, uint8_t cnt)
{
	uint8_t iError = 0;
	esp_err_t espRc;

	  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            if (reg_addr != -1) {
                i2c_master_write_byte(cmd, dev_addr << 1 | WRITE_BIT, ACK_CHECK_EN);
                i2c_master_write_byte(cmd, reg_addr, ACK_CHECK_EN);
                i2c_master_start(cmd);
            }
            i2c_master_write_byte(cmd, dev_addr << 1 | READ_BIT, ACK_CHECK_EN);
            if (cnt > 1) {
                i2c_master_read(cmd, reg_data, cnt - 1, ACK_VAL);
            }
            i2c_master_read_byte(cmd, reg_data + cnt - 1, NACK_VAL);
            i2c_master_stop(cmd);
            esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_PERIOD_MS);
            i2c_cmd_link_delete(cmd);
	        return iError;
}

void app_main() {
    esp_err_t ret;
    uint8_t rx_data[5];
    int8_t i2cRet;
    uint8_t *press = malloc(2*sizeof(uint8_t));
    uint8_t *temp = malloc(2*sizeof(uint8_t));
    
    peripherical_init();

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = wifi_init();
    if(ret != ESP_OK){
        ESP_LOGI(TAG, "Wifi init ERROR !!!\n");    
    }
    ESP_LOGI(TAG, "ESP_WIFI_INIT\n");
    ESP_ERROR_CHECK(ret);
    
    ret = web_server();
    if(ret != ESP_OK){
        ESP_LOGI(TAG, "web server ERROR !!!\n");    
    }
    ESP_LOGI(TAG, "DALE\n");
    ESP_ERROR_CHECK(ret);

    
    // if(ret != ESP_OK){
    //     ESP_LOGI(TAG, "I2C Error !!!\n");    
    // }
    // ESP_LOGI(TAG, "I2C read mode active !!!\n"); 
    i2c_write()
    while(1){
        vTaskDelay(200);
        i2cRet = i2c_read(BMP280_ADDR,0xF3, press, 2);
        ESP_LOG_BUFFER_HEX(TAG, press, 2);
        i2cRet = i2c_read(BMP280_ADDR,0xF4, temp,2);
        ESP_LOG_BUFFER_HEX(TAG, temp, 2);
        ESP_LOGI(TAG,"-----------");
    }

    iot_servo_deinit(LEDC_LOW_SPEED_MODE);
}