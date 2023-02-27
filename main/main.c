#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "sys/socket.h"
#include <dirent.h>

#include <rom/ets_sys.h>
#include <esp_types.h>
#include "esp_timer.h"
#include <esp_task_wdt.h>
#include <esp_rom_gpio.h>
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include <esp_wifi.h>
#include "mdns.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_http_server.h"
#include "esp_camera.h"
#include <ff.h>
#include "protocol_examples_common.h"

#include "driver/i2c.h"
#include "driver/ledc.h"
#include "iot_servo.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "cJSON.h"
#include "bme280.h"
#include "ADS1115.h"

#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"

// ************ DEFINE WHAT MOTOR WILL USE **********************//

// #define STEPPER_DRIVER
// #define STEP_ULN2003
// #define SERVO_MOTOR

//***************************************************************//

#define CAM_PIN_PWDN -1  // power down is not used
#define CAM_PIN_RESET -1 // software reset will be performed
#define CAM_PIN_XCLK 18
#define CAM_PIN_SIOD 5
#define CAM_PIN_SIOC 4
#define CAM_PIN_D7 36
#define CAM_PIN_D6 39
#define CAM_PIN_D5 34
#define CAM_PIN_D4 35
#define CAM_PIN_D3 32
#define CAM_PIN_D2 33
#define CAM_PIN_D1 25
#define CAM_PIN_D0 26
#define CAM_PIN_VSYNC 13
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 19
#define CONFIG_XCLK_FREQ 8000000
#define PART_BOUNDARY "123456789000000000000987654321"

#define MAX_WEB_SOCKETS 6

#define MICROSTEP 1
#define MAX_STEP 4096
#define DEGREE_PER_STEP 360 / MAX_STEP
#define STEP_COMPENSATOR 36
#define COIL1 32
#define COIL2 33
#define COIL3 25
#define COIL4 26
#define H_BRIDGE_1 27
#define H_BRIDGE_2 14

#define SERVO_CH0_PIN 12

#define EXAMPLE_ESP_WIFI_SSID "VIVOFIBRA-EBB0"
#define EXAMPLE_ESP_WIFI_PASS "C1A75F2A3B"
#define EXAMPLE_ESP_MAXIMUM_RETRY 10

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

#define I2C_MASTER_FREQ_HZ 40000
#define I2C_MASTER_SDA_IO 21
#define I2C_MASTER_SCL_IO 22
#define AS5600 0x36
#define REG12 0X0C
#define REG13 0X0D
// #define BMP280_ADDR 0x76

#define ADS1115 0x48
#define ADS1115_LSB_SIZE 0.0001875

#define I2C_MASTER_TX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define WRITE_BIT I2C_MASTER_WRITE  /*!< I2C master write */
#define READ_BIT I2C_MASTER_READ    /*!< I2C master read */
#define ACK_CHECK_EN 0x1            /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0           /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x0                 /*!< I2C ack value */
#define NACK_VAL 0x1                /*!< I2C nack value */

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)
#define SCRATCH_BUFSIZE 8192

static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

void i2c_write(uint8_t dev_addr, uint8_t reg_addr, uint8_t *reg_data, uint8_t cnt);
uint8_t i2c_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *reg_data, uint8_t cnt);
void BME280_delay_msek(u32 msek);
static esp_err_t init_camera(void);

static httpd_handle_t server = NULL;
static QueueHandle_t out_msg_queue;
static QueueHandle_t incoming_client_events;
SemaphoreHandle_t new_client_semaphore;
int list_of_sockets[MAX_WEB_SOCKETS];

struct async_resp_arg
{
    httpd_handle_t hd;
    int fd;
};

typedef struct
{
    int socket_fd;
    int last_socket_fd;
    void *data_ptr;

} cb_socket_args_t;

typedef struct
{
    uint16_t type;
    uint16_t x;
    uint16_t y;
} __attribute__((packed)) client_event_t;

struct file_server_data
{
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
};

struct bme280_t bme280;
ads1115_t ads1115_cfg;
camera_config_t camera_config;

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
static const char *TAG = "wifi station";
static ledc_channel_config_t ledc_channel;

int16_t pos_count = 0;
int8_t dir_step = 0;
uint8_t angle = 0;

int16_t step_position = 0;
int8_t speed = 0; // velocidade varia de -100% até 100%
float batteryLevel = 0;
float temperature = 0;

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
        return httpd_resp_set_type(req, "application/javascript");
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
    else if (IS_FILE_EXT(filename, ".txt"))
    {
        ESP_LOGI(TAG, "file is a .txt");
        return httpd_resp_set_type(req, "text/plain");
    }
    /* This is a limited set only */
    /* For any other type always set as plain text */
    return httpd_resp_set_type(req, "text/plain");
}

void move_direction(int16_t step)
{
    // se step - pos count--
    //  * Step C0 C1 C2 C3
    //  *    1  1  0  0  0
    //  *    2  0  1  0  0
    //  *    3  0  0  1  0
    //  *    4  0  0  0  1

    // for 1/2 step - pos count--
    //  * Step C0 C1 C2 C3
    //  *    1  1  0  0  0
    //  *    2  1  1  0  0
    //  *    3  0  1  0  0
    //  *    4  0  1  1  0
    //  *    5  0  0  1  0
    //  *    6  0  0  1  1
    //  *    7  0  0  0  1
    //  *    8  1  0  0  1

    pos_count += step;

    if (pos_count < 0)
        pos_count = MAX_STEP;
    else if (pos_count >= MAX_STEP)
        pos_count = 0;

    // ESP_LOGI(TAG, "step");
    vTaskDelay(10 / portTICK_PERIOD_MS);

    switch (pos_count % 8)
    {
    case 0:
        gpio_set_level(COIL1, 1);
        gpio_set_level(COIL2, 0);
        gpio_set_level(COIL3, 0);
        gpio_set_level(COIL4, 0);
        break;
    case 1:
        gpio_set_level(COIL1, 1);
        gpio_set_level(COIL2, 1);
        gpio_set_level(COIL3, 0);
        gpio_set_level(COIL4, 0);
        break;
    case 2:
        gpio_set_level(COIL1, 0);
        gpio_set_level(COIL2, 1);
        gpio_set_level(COIL3, 0);
        gpio_set_level(COIL4, 0);
        break;
    case 3:
        gpio_set_level(COIL1, 0);
        gpio_set_level(COIL2, 1);
        gpio_set_level(COIL3, 1);
        gpio_set_level(COIL4, 0);
        break;
    case 4:
        gpio_set_level(COIL1, 0);
        gpio_set_level(COIL2, 0);
        gpio_set_level(COIL3, 1);
        gpio_set_level(COIL4, 0);
        break;
    case 5:
        gpio_set_level(COIL1, 0);
        gpio_set_level(COIL2, 0);
        gpio_set_level(COIL3, 1);
        gpio_set_level(COIL4, 1);
        break;
    case 6:
        gpio_set_level(COIL1, 0);
        gpio_set_level(COIL2, 0);
        gpio_set_level(COIL3, 0);
        gpio_set_level(COIL4, 1);
        break;
    default:
        gpio_set_level(COIL1, 1);
        gpio_set_level(COIL2, 0);
        gpio_set_level(COIL3, 0);
        gpio_set_level(COIL4, 1);
        break;
    }
}

static const char *get_path_from_uri(char *dest, const char *base_path, const char *uri, size_t destsize)
{
    const size_t base_pathlen = strlen(base_path);
    size_t pathlen = strlen(uri);

    const char *quest = strchr(uri, '?');
    if (quest)
    {
        pathlen = MIN(pathlen, quest - uri);
    }
    const char *hash = strchr(uri, '#');
    if (hash)
    {
        pathlen = MIN(pathlen, hash - uri);
    }

    if (base_pathlen + pathlen + 1 > destsize)
    {
        /* Full path string won't fit into destination buffer */
        return NULL;
    }

    /* Construct full path (base + path) */
    strcpy(dest, base_path);
    strlcpy(dest + base_pathlen, uri, pathlen + 1);

    /* Return pointer to path, skipping the base */
    return dest + base_pathlen;
}

static esp_err_t root_handler(httpd_req_t *req)
{
    extern const uint8_t index_html_start[] asm("_binary_page_html_start"); // uint8_t
    extern const uint8_t index_html_end[] asm("_binary_page_html_end");     // uint8_t

    httpd_resp_set_type(req, "page.html");
    httpd_resp_send(req, (char *)index_html_start, index_html_end - index_html_start - 1);

    return ESP_OK;
}

static esp_err_t camera_handler_function(httpd_req_t *req)
{
    if (init_camera() != ESP_OK)
        ESP_LOGE(TAG, "camera init ERROR !!!");
    else
        ESP_LOGI(TAG, "camera init success");

    camera_fb_t *fb = NULL;
    fb = esp_camera_fb_get();
    if (!fb)
    {
        ESP_LOGE(TAG, "Camera capture failed errno:%d - %s", errno, strerror(errno));
        extern const uint8_t unavailable_start[] asm("_binary_unavailable_jpeg_start"); // uint8_t
        extern const uint8_t unavailable_end[] asm("_binary_unavailable_jpeg_end");     // uint8_t
        httpd_resp_set_type(req, "image/jpeg");
        httpd_resp_send(req, (char *)unavailable_start, unavailable_end - unavailable_start - 1);
        esp_camera_fb_return(fb);
    }
    else
    {
        ESP_LOGI(TAG, "Capture success");
        ESP_LOGI(TAG, "Format:%d - %d bytes", fb->format, fb->len);
        uint8_t *out;
        size_t out_len;
        int quality = 100;
        fmt2jpg(fb->buf, fb->len, fb->width, fb->height, fb->format, quality, &out, &out_len);
        ESP_LOGI(TAG, "JPEG ptr: %p, jpeg_len:%d", out, out_len);
        esp_camera_fb_return(fb);

        httpd_resp_set_type(req, "image/jpeg");
        httpd_resp_set_status(req, HTTPD_200);
        int chunk_size = 1 * 1024;
        for (int i = 0; i < out_len; i += chunk_size)
        {
            int bytes_to_send = chunk_size;
            if (i + chunk_size > out_len)
            {
                bytes_to_send = out_len - i + 1;
            }
            ESP_LOGI(TAG, "Sending %d/%d", i, out_len);
            httpd_resp_send_chunk(req, (char *)out + i, bytes_to_send);
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        httpd_resp_send_chunk(req, NULL, 0);

        free(out);
    }

    ESP_LOGI(TAG, "%d req: %s", __LINE__, (char *)req->uri);
    if (esp_camera_deinit() != ESP_OK)
        ESP_LOGE(TAG, "camera deinit ERROR !!!");
    else
        ESP_LOGI(TAG, "camera deinit success");
    return ESP_OK;
}

static esp_err_t root_assets_handler(httpd_req_t *req)
{
    if (strcasecmp((char *)req->uri, "/assets/Fortron.png") == 0)
    {
        extern const uint8_t Fortron_start[] asm("_binary_Fortron_png_start"); // uint8_t
        extern const uint8_t Fortron_end[] asm("_binary_Fortron_png_end");     // uint8_t
        set_content_type_from_file(req, (char *)req->uri);
        httpd_resp_send(req, (char *)Fortron_start, Fortron_end - Fortron_start - 1);
        ESP_LOGI(TAG, "%d req: %s", __LINE__, (char *)req->uri);
    }
    else if (strcasecmp((char *)req->uri, "/assets/unavailable.jpeg") == 0)
    {
        extern const uint8_t unavailable_start[] asm("_binary_unavailable_jpeg_start"); // uint8_t
        extern const uint8_t unavailable_end[] asm("_binary_unavailable_jpeg_end");     // uint8_t
        set_content_type_from_file(req, (char *)req->uri);
        httpd_resp_send(req, (char *)unavailable_start, unavailable_end - unavailable_start - 1);
        ESP_LOGI(TAG, "%d req: %s", __LINE__, (char *)req->uri);
    }
    else if (strcasecmp((char *)req->uri, "/assets/trator.gif") == 0)
    {
        extern const uint8_t trator_start[] asm("_binary_trator_gif_start"); // uint8_t
        extern const uint8_t trator_end[] asm("_binary_trator_gif_end");     // uint8_t
        set_content_type_from_file(req, (char *)req->uri);
        httpd_resp_send(req, (char *)trator_start, trator_end - trator_start - 1);
        ESP_LOGI(TAG, "%d req: %s", __LINE__, (char *)req->uri);
    }
    else if (strcasecmp((char *)req->uri, "/assets/style.css") == 0)
    {
        extern const uint8_t style_start[] asm("_binary_style_css_start"); // uint8_t
        extern const uint8_t style_end[] asm("_binary_style_css_end");     // uint8_t
        set_content_type_from_file(req, (char *)req->uri);
        httpd_resp_send(req, (char *)style_start, style_end - style_start - 1);
        ESP_LOGI(TAG, "%d req: %s", __LINE__, (char *)req->uri);
    }
    else if (strcasecmp((char *)req->uri, "/assets/code.js") == 0)
    {
        extern const uint8_t code_start[] asm("_binary_code_js_start"); // uint8_t
        extern const uint8_t code_end[] asm("_binary_code_js_end");     // uint8_t
        set_content_type_from_file(req, (char *)req->uri);
        httpd_resp_send(req, (char *)code_start, code_end - code_start - 1);
        ESP_LOGI(TAG, "%d req: %s", __LINE__, (char *)req->uri);
    }
    else if (strcasecmp((char *)req->uri, "/assets/F.ico") == 0)
    {
        extern const uint8_t F_start[] asm("_binary_F_ico_start"); // uint8_t
        extern const uint8_t F_end[] asm("_binary_F_ico_end");     // uint8_t
        set_content_type_from_file(req, (char *)req->uri);
        httpd_resp_send(req, (char *)F_start, F_end - F_start - 1);
        ESP_LOGI(TAG, "%d req: %s", __LINE__, (char *)req->uri);
    }
    else if (strcasecmp((char *)req->uri, "/assets/jquery-3.6.3.min.js") == 0)
    {
        extern const uint8_t jquery_start[] asm("_binary_jquery_3_6_3_min_js_start"); // uint8_t
        extern const uint8_t jquery_end[] asm("_binary_jquery_3_6_3_min_js_end");     // uint8_t
        set_content_type_from_file(req, (char *)req->uri);
        httpd_resp_send(req, (char *)jquery_start, jquery_end - jquery_start - 1);
        ESP_LOGI(TAG, "%d req: %s", __LINE__, (char *)req->uri);
    }
    else if (strcasecmp((char *)req->uri, "/assets/sensors/BatteryLevel/lvl.txt") == 0)
    {
        extern const uint8_t batLvL_start[] asm("_binary_lvl_txt_start"); // uint8_t
        extern const uint8_t batLvL_end[] asm("_binary_lvl_txt_end");     // uint8_t
        set_content_type_from_file(req, (char *)req->uri);
        httpd_resp_send(req, (char *)batLvL_start, batLvL_end - batLvL_start - 1);
        ESP_LOGI(TAG, "%d req: %s", __LINE__, (char *)req->uri);
    }
    return ESP_OK;
}

static esp_err_t root_sensors_handler(httpd_req_t *req)
{
    if (strcasecmp((char *)req->uri, "/sensors/BatteryLevel") == 0)
    {
        uint16_t adc_read = 0;
        ADS1115_request_single_ended_AIN1();
        while (!ADS1115_get_conversion_state())
            vTaskDelay(1 / portTICK_PERIOD_MS);
        adc_read = ADS1115_get_conversion();
        batteryLevel = adc_read * ADS1115_LSB_SIZE;
        ESP_LOGI(TAG, "BatLvL: %.2f V", batteryLevel);
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "BatLvL", batteryLevel);

        char *buff = malloc(sizeof(resp) + 1);
        memset(buff, 0x00, sizeof(resp) + 1);

        buff = cJSON_Print(resp);

        httpd_resp_send(req, buff, strlen(buff));
        cJSON_Delete(resp);
        free(buff);
    }
    else if (strcasecmp((char *)req->uri, "/sensors/Temperature") == 0)
    {
        s32 com_rslt;
        s32 v_uncomp_pressure_s32;
        s32 v_uncomp_temperature_s32;
        s32 v_uncomp_humidity_s32;

        com_rslt = bme280_read_uncomp_pressure_temperature_humidity(&v_uncomp_pressure_s32, &v_uncomp_temperature_s32, &v_uncomp_humidity_s32);

        if (com_rslt == SUCCESS)
        {
            double temp = bme280_compensate_temperature_double(v_uncomp_temperature_s32);
            temperature = (float)temp;
            ESP_LOGI(TAG, "temp: %.2f", temp);
            cJSON *resp = cJSON_CreateObject();
            cJSON_AddNumberToObject(resp, "Temp", temperature);

            char *buff = malloc(sizeof(resp) + 1);
            memset(buff, 0x00, sizeof(resp) + 1);

            buff = cJSON_Print(resp);

            httpd_resp_send(req, buff, strlen(buff));
            cJSON_Delete(resp);
            free(buff);
        }
        else
        {
            ESP_LOGI(TAG, "BME Bad reading: %d", com_rslt);
        }
    }
    else if (strcasecmp((char *)req->uri, "/sensors/Speed") == 0)
    {
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "Speed", speed);

        char *buff = malloc(sizeof(resp) + 1);
        memset(buff, 0x00, sizeof(resp) + 1);

        buff = cJSON_Print(resp);

        httpd_resp_send(req, buff, strlen(buff));
        cJSON_Delete(resp);
        free(buff);
    }
    return ESP_OK;
}

static void initialise_mdns(void)
{
    // char* hostname = generate_hostname();
    char hostname[] = "experimento_kanedistico";
    // initialize mDNS
    ESP_ERROR_CHECK(mdns_init());
    // set mDNS hostname (required if you want to advertise services)
    ESP_ERROR_CHECK(mdns_hostname_set(hostname));
    ESP_LOGI(TAG, "mdns hostname set to: [%s]", hostname);
    // set default mDNS instance name
    ESP_ERROR_CHECK(mdns_instance_name_set("teste"));

    mdns_service_add("Interface WEB", "_http", "_tcp", 80, NULL, 0);
}

void last_frame_callback(esp_err_t err, int socket, void *arg)
{
    cb_socket_args_t *cb_args = (cb_socket_args_t *)arg;
    if (cb_args->socket_fd == cb_args->last_socket_fd)
    {
        ESP_LOGI(TAG, "frame callback was ");
        // heap_caps_free(cb_args->data_ptr);
    }
    free(arg);
}
/*
void img_stream(void *args)
{
    camera_fb_t *fb = NULL;
    httpd_ws_frame_t ws_pkt;
    uint8_t *data;
    uint8_t n_clients = 0;
    while (1)
    {
        n_clients = 0;

        vTaskDelay(1000);
        int last_socket_id = -1;
        for (int i = 0; i < MAX_WEB_SOCKETS; i++)
        {
            int fd = list_of_sockets[i];
            ESP_LOGI(TAG, "socket:%d  number:%d", fd, i);
            if (fd != 0)
            {
                httpd_ws_client_info_t info = httpd_ws_get_fd_info(server, fd);
                ESP_LOGI(TAG, " info:%d", info);
                if (info == HTTPD_WS_CLIENT_WEBSOCKET)
                {
                    last_socket_id = fd;
                }
                else
                {
                }
            }
        }

        ESP_LOGI(TAG, "last_socket_id:%d", last_socket_id);

        if (last_socket_id == -1)
        {
            continue;
        }

        fb = esp_camera_fb_get();

        if (!fb)
        {
            ESP_LOGE(TAG, "Camera capture failed");
            vTaskDelay(500);
            continue;
        }
        else
        {
            ESP_LOGI(TAG, "Capture success");
        }

        memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

        ws_pkt.payload = fb->buf;
        ws_pkt.len = fb->len;
        ws_pkt.type = HTTPD_WS_TYPE_BINARY;

        ESP_LOGI(TAG, "JPG: %luKB", (uint32_t)(fb->len / 1024));

        for (int i = 0; i < MAX_WEB_SOCKETS; i++)
        {
            int fd = list_of_sockets[i];

            if (fd != 0)
            {
                httpd_ws_client_info_t info = httpd_ws_get_fd_info(server, fd);
                ESP_LOGI(TAG, " info:%d", info);
                if (info == HTTPD_WS_CLIENT_WEBSOCKET)
                {
                    cb_socket_args_t *args = malloc(sizeof(cb_socket_args_t));
                    args->socket_fd = fd;
                    args->last_socket_fd = last_socket_id;
                    args->data_ptr = ws_pkt.payload;

                    char buffer[1024];
                    size_t bytes_read;
                    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fb->buf)) > 0)
                    {
                        esp_serial_write(serial_port, buffer, bytes_read);
                    }
                    esp_serial_destroy(serial_port);

                    // esp_err_t err = httpd_ws_send_data_async(server, fd, &ws_pkt, &last_frame_callback, args);
                    esp_err_t err = ESP_OK

                        ESP_LOGI(TAG, "Sending %d bytes to client %d x:%d y:%d", ws_pkt.len, i, fb->width, fb->height);
                    if (err != ESP_OK)
                    {
                        for (int i = 0; i < 10; i++)
                        {
                            ESP_LOGE(TAG, "Error sending frame to client %d", i);
                        }

                        if (fd == last_socket_id)
                        {
                            ESP_LOGE(TAG, "Last socket, freeing data");
                            free(args);
                        }
                    }
                    else
                    {
                        // ESP_LOGI(TAG, "%d bytes sent to client %d", ws_pkt.len, i);
                        ESP_LOGI(TAG, "data send");
                        n_clients++;
                    }
                }
            }
        }
        if (n_clients == 0)
        {
            ESP_LOGI(TAG, "No clients connected");
        }
        esp_camera_fb_return(fb);
        ESP_LOGI(TAG, "freeing camera buffer");
    }
}
*/

static void ws_async_send(void *arg)
{
    static const char *data = "Async data";
    struct async_resp_arg *resp_arg = arg;
    httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t *)data;
    ws_pkt.len = strlen(data);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    httpd_ws_send_frame_async(hd, fd, &ws_pkt);
    free(resp_arg);
}

static esp_err_t trigger_async_send(httpd_handle_t handle, httpd_req_t *req)
{
    struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
    resp_arg->hd = req->handle;
    resp_arg->fd = httpd_req_to_sockfd(req);
    return httpd_queue_work(handle, ws_async_send, resp_arg);
}

static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET)
    {
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        return ESP_OK;
    }
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ESP_LOGW(TAG, "Request type: %d - len:%d", req->method, req->content_len);

    ws_pkt.type = HTTPD_WS_TYPE_BINARY;
    // determine the actual packet length
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed: %s", esp_err_to_name(ret));
        return ret;
    }
    int buffer_len = ws_pkt.len + 1;
    ws_pkt.payload = malloc(buffer_len);

    ret = httpd_ws_recv_frame(req, &ws_pkt, buffer_len);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
        return ESP_OK;
    }
    if (ws_pkt.len >= sizeof(client_event_t))
    {
        client_event_t *event = (client_event_t *)ws_pkt.payload;
        ESP_LOGW(TAG, "Event type: %d - x:%d - y:%d", event->type, event->x, event->y);
        if (xQueueSend(incoming_client_events, event, pdMS_TO_TICKS(100)) != pdPASS)
        {
            ESP_LOGE(TAG, "Failed to send client event to queue");
        }
    }
    else
    {
        ESP_LOGW(TAG, "Received less data then expected");
    }

    return ESP_OK;
}

static esp_err_t direction_handler(httpd_req_t *req)
{
    int8_t i2cRet;
    uint8_t *ang1 = malloc(sizeof(uint8_t));
    uint8_t *ang2 = malloc(sizeof(uint8_t));
    uint16_t angle = 0;
    memset(ang1, 0x00, sizeof(uint8_t));
    memset(ang2, 0x00, sizeof(uint8_t));

    int16_t steps;
    // tamanho do conteúdo recebido pelo POST
    uint8_t len = req->content_len * sizeof(char *) + 1;

    // Alocação dinâmica do conteúdo recebido da requisição POST
    // LEMBRAR DE LIBERAR A MEMÓRIA FREE(VAR) !!!!!!!!
    char *content = malloc(len);
    memset(content, 0x00, len);

    // Recebe o conteúdo
    httpd_req_recv(req, content, req->content_len);
    // ESP_LOGI(TAG, "");

    // Reconstrói o JSON
    cJSON *direction = cJSON_Parse(content);

    // step recebe apenas o valor do objeto x, como int
    steps = cJSON_GetObjectItem(direction, "x")->valueint;
    speed = cJSON_GetObjectItem(direction, "y")->valueint;

    ESP_LOGI(TAG, "steps: %d", steps);
    ESP_LOGI(TAG, "speed: %d", speed);
    if (speed > 0)
    {
        gpio_set_level(H_BRIDGE_1, 1);
        gpio_set_level(H_BRIDGE_2, 0);
    }
    else if (speed < 0)
    {
        gpio_set_level(H_BRIDGE_1, 0);
        gpio_set_level(H_BRIDGE_2, 1);
    }
    else
    {
        gpio_set_level(H_BRIDGE_1, 0);
        gpio_set_level(H_BRIDGE_2, 0);
    }

    ESP_LOGI(TAG, "steps: %d", steps);
    if (abs(steps) != 0)
    {
        // Como saber a condição inicial, no caso a última direção antes de ligar?
        if (steps / abs(steps) != dir_step)
        {
            if (steps / abs(steps) == -1)
            {
                dir_step = -1;
                steps -= STEP_COMPENSATOR;
            }
            else
            {
                dir_step = 1;
                steps += STEP_COMPENSATOR;
            }
        }

        ESP_LOGI(TAG, "steps: %d, calc: %d", steps, steps / abs(steps));
        for (uint16_t i = 0; i < abs(steps); i++)
        {
            move_direction(steps / abs(steps));
        }
    }
    // vTaskDelay(20 / portTICK_PERIOD_MS);
    // i2cRet = i2c_read(AS5600, REG13, ang1, 1);
    // i2cRet = i2c_read(AS5600, REG12, ang2, 1);
    // angle = (*ang2 << 8) | (*ang1);
    // ESP_LOGI(TAG, "%.12f;%.12f", (float)angle * 360 / 4095, (float)pos_count * DEGREE_PER_STEP / MICROSTEP);

    // Cria objeto JSON para enviar como resposta
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp, "x", (float)pos_count * DEGREE_PER_STEP / MICROSTEP);

    // alocação do buffer
    // LEMBRAR DE LIBERAR A MEMÓRIA FREE(VAR) !!!!!!!
    char *buff = malloc(sizeof(resp) + 1);
    memset(buff, 0x00, sizeof(resp) + 1);

    // passa todo o JSON como string
    buff = cJSON_Print(resp);

    // envia essa lindeza finalmente
    httpd_resp_send(req, buff, strlen(buff));
    cJSON_Delete(direction);
    cJSON_Delete(resp);
    free(content);
    free(buff);
    return ESP_OK;
}

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGE(TAG, "connect to the AP fail");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifi_init(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
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

    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);

    return ESP_OK;
}

esp_err_t connection_open_cb(httpd_handle_t s_h, int sock)
{
    httpd_ws_client_info_t info = httpd_ws_get_fd_info(s_h, sock);
    ESP_LOGI(TAG, "Socket %d has been opened info:%d", sock, info);

    for (int i = 0; i < MAX_WEB_SOCKETS; i++)
    {
        if (list_of_sockets[i] == 0)
        {
            list_of_sockets[i] = sock;

            return ESP_OK;
        }
    }

    return ESP_FAIL;
};

void connection_close_cb(httpd_handle_t s_h, int sock)
{
    for (int i = 0; i < MAX_WEB_SOCKETS; i++)
    {
        if (list_of_sockets[i] == sock)
        {
            list_of_sockets[i] = 0;
        }
    }
    ESP_LOGI(TAG, "Socket %d has been closed", sock);

    struct linger so_linger;
    so_linger.l_onoff = true;
    so_linger.l_linger = 0;
    int rv = setsockopt(sock, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger));
    if (rv < 0)
    {
        ESP_LOGE(TAG, "lwip_setsockopt() failed. fd %d rv %d errno %d", sock, rv, errno);
    }

    rv = close(sock);
    if (rv != 0)
    {
        ESP_LOGE(TAG, "close() fd (%d) failed. rv %d errno %d", sock, rv, errno);
    }

    /*
    if (esp_camera_deinit() != ESP_OK)
        ESP_LOGE(TAG, "camera deinit ERROR !!!");
    else
        ESP_LOGI(TAG, "camera deinit success");
    */
};

httpd_uri_t direction_uri = {
    .uri = "/direction",
    .method = HTTP_POST,
    .handler = direction_handler,
    .user_ctx = NULL,
    .is_websocket = false,
};

httpd_uri_t root_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_handler,
    .user_ctx = NULL,
    .is_websocket = false,
};

httpd_uri_t root_assets_uri = {
    .uri = "/assets/*",
    .method = HTTP_GET,
    .handler = root_assets_handler,
    .user_ctx = NULL,
    .is_websocket = false,
};

httpd_uri_t root_sensors_uri = {
    .uri = "/sensors/*",
    .method = HTTP_GET,
    .handler = root_sensors_handler,
    .user_ctx = NULL,
    .is_websocket = false,
};

httpd_uri_t camera_handler = {
    .uri = "/camera",
    .method = HTTP_GET,
    .handler = camera_handler_function,
    .is_websocket = false,
    .user_ctx = NULL,
};

httpd_uri_t root_ws_start_uri = {
    .uri = "/ws",
    .method = HTTP_GET,
    .handler = ws_handler,
    .user_ctx = NULL,
    .is_websocket = true,
};

static httpd_handle_t web_server()
{
    memset(list_of_sockets, 0, sizeof(int) * MAX_WEB_SOCKETS);
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_open_sockets = MAX_WEB_SOCKETS;
    config.stack_size = 8192;
    // config.recv_wait_timeout = 10; // Timeout for recv function (in seconds)
    // config.send_wait_timeout = 10;
    // config.open_fn = connection_open_cb;
    // config.close_fn = connection_close_cb;
    // config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK)
    {
        ESP_LOGI(TAG, "WEB Server initialized sus !!!\n");
        // httpd_register_uri_handler(server, &root_sensors_uri);
        // httpd_register_uri_handler(server, &root_assets_uri);
        // httpd_register_uri_handler(server, &direction_uri);
        // httpd_register_uri_handler(server, &root_uri);
        // httpd_register_uri_handler(server, &root_ws_start_uri);
        httpd_register_uri_handler(server, &camera_handler);
        return server;
    }

    ESP_LOGE(TAG, "WEB Server failed !!!\n");
    return NULL;
}

static esp_err_t stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    return httpd_stop(server);
}

static esp_err_t init_camera(void)
{
    camera_config.pin_pwdn = CAM_PIN_PWDN;
    camera_config.pin_reset = CAM_PIN_RESET;
    camera_config.pin_xclk = CAM_PIN_XCLK;
    camera_config.pin_sccb_sda = CAM_PIN_SIOD;
    camera_config.pin_sccb_scl = CAM_PIN_SIOC;

    camera_config.pin_d7 = CAM_PIN_D7;
    camera_config.pin_d6 = CAM_PIN_D6;
    camera_config.pin_d5 = CAM_PIN_D5;
    camera_config.pin_d4 = CAM_PIN_D4;
    camera_config.pin_d3 = CAM_PIN_D3;
    camera_config.pin_d2 = CAM_PIN_D2;
    camera_config.pin_d1 = CAM_PIN_D1;
    camera_config.pin_d0 = CAM_PIN_D0;
    camera_config.pin_vsync = CAM_PIN_VSYNC;
    camera_config.pin_href = CAM_PIN_HREF;
    camera_config.pin_pclk = CAM_PIN_PCLK;

    camera_config.xclk_freq_hz = CONFIG_XCLK_FREQ;
    camera_config.ledc_timer = LEDC_TIMER_0;
    camera_config.ledc_channel = LEDC_CHANNEL_0;

    camera_config.pixel_format = PIXFORMAT_RGB565;
    camera_config.frame_size = FRAMESIZE_VGA;

    camera_config.jpeg_quality = 15;
    camera_config.fb_count = 5;
    camera_config.grab_mode = CAMERA_GRAB_WHEN_EMPTY; // CAMERA_GRAB_LATEST. Sets when buffers should be filled
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failure camera_init %d %s", errno, strerror(errno));
        return err;
    }
    return ESP_OK;
}

void peripherical_init()
{
    esp_err_t err;

    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = 50,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .clk_cfg = LEDC_AUTO_CLK,
    };

    if (ledc_timer_config(&ledc_timer) != ESP_OK)
        ESP_LOGE(TAG, "Timer ERROR !!!");
    else
        ESP_LOGI(TAG, "Timer init OK!");

    ledc_channel.channel = LEDC_CHANNEL_0;
    ledc_channel.duty = 0;
    ledc_channel.gpio_num = 2;
    ledc_channel.speed_mode = LEDC_LOW_SPEED_MODE;
    ledc_channel.hpoint = 0;
    ledc_channel.timer_sel = LEDC_TIMER_0;

    if (ledc_channel_config(&ledc_channel) != ESP_OK)
        ESP_LOGE(TAG, "PWM ERROR !!!");
    else
        ESP_LOGI(TAG, "PWM init OK!");

    // servo_config_t servo_cfg = {
    //     .max_angle = 180,
    //     .min_width_us = 500,
    //     .max_width_us = 2500,
    //     .freq = 50,
    //     .timer_number = LEDC_TIMER_0,
    //     .channels = {
    //         .servo_pin = {
    //             SERVO_CH0_PIN,
    //         },
    //         .ch = {
    //             LEDC_CHANNEL_0,
    //         },
    //     },
    //     .channel_number = 1,
    // };
    // iot_servo_init(LEDC_LOW_SPEED_MODE, &servo_cfg);

    gpio_set_direction(COIL1, GPIO_MODE_OUTPUT);
    gpio_set_direction(COIL2, GPIO_MODE_OUTPUT);
    gpio_set_direction(COIL3, GPIO_MODE_OUTPUT);
    gpio_set_direction(COIL4, GPIO_MODE_OUTPUT);
    gpio_set_direction(H_BRIDGE_1, GPIO_MODE_OUTPUT);
    gpio_set_direction(H_BRIDGE_2, GPIO_MODE_OUTPUT);

    gpio_set_level(COIL1, 0);
    gpio_set_level(COIL2, 0);
    gpio_set_level(COIL3, 0);
    gpio_set_level(COIL4, 0);
    gpio_set_level(H_BRIDGE_1, 0);
    gpio_set_level(H_BRIDGE_2, 0);

    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0));
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO, // select GPIO specific to your project
        .scl_io_num = I2C_MASTER_SCL_IO, // select GPIO specific to your project
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ, // select frequency specific to your project
        .clk_flags = 0,                         // you can use I2C_SCLK_SRC_FLAG_* flags to choose i2c source clock here
    };
    i2c_param_config(I2C_NUM_0, &conf);

    bme280.bus_write = i2c_write;
    bme280.bus_read = i2c_read;
    bme280.dev_addr = BME280_I2C_ADDRESS1;
    bme280.delay_msec = BME280_delay_msek;

    s32 com_rslt;
    /*
    com_rslt = bme280_init(&bme280);
    ESP_LOGI(TAG, "bme init: %d", com_rslt);
    com_rslt += bme280_set_oversamp_pressure(BME280_OVERSAMP_16X);
    ESP_LOGI(TAG, "bme press: %d", com_rslt);
    com_rslt += bme280_set_oversamp_temperature(BME280_OVERSAMP_2X);
    ESP_LOGI(TAG, "bme temp: %d", com_rslt);

    com_rslt += bme280_set_power_mode(BME280_NORMAL_MODE);
    ESP_LOGI(TAG, "bme pwr mode: %d", com_rslt);
    if (com_rslt == SUCCESS)
        ESP_LOGI(TAG, "BME280 init!");
    else
        ESP_LOGE(TAG, "BME280 ERROR !!!");

    ads1115_cfg.reg_cfg = ADS1115_CFG_LS_COMP_MODE_TRAD | // Comparator is traditional
                          ADS1115_CFG_LS_COMP_LAT_NON |   // Comparator is non-latching
                          ADS1115_CFG_LS_COMP_POL_LOW |   // Alert is active low
                          ADS1115_CFG_LS_COMP_QUE_DIS |   // Compator is disabled
                          ADS1115_CFG_LS_DR_1600SPS |     // No. of samples to take
                          ADS1115_CFG_MS_MODE_SS;
    ads1115_cfg.dev_addr = 0x48;
    ADS1115_initiate(&ads1115_cfg);
    */
}

void BME280_delay_msek(u32 msek)
{
    vTaskDelay(msek / portTICK_PERIOD_MS);
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
    if (reg_addr != -1)
    {
        i2c_master_write_byte(cmd, dev_addr << 1 | WRITE_BIT, ACK_CHECK_EN);
        i2c_master_write_byte(cmd, reg_addr, ACK_CHECK_EN);
        i2c_master_start(cmd);
    }
    i2c_master_write_byte(cmd, dev_addr << 1 | READ_BIT, ACK_CHECK_EN);
    if (cnt > 1)
    {
        i2c_master_read(cmd, reg_data, cnt - 1, ACK_VAL);
    }
    i2c_master_read_byte(cmd, reg_data + cnt - 1, NACK_VAL);
    i2c_master_stop(cmd);
    espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return iError;
}

void app_main()
{
    esp_err_t ret;

    peripherical_init();

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // out_msg_queue = xQueueCreate(100, sizeof(uint8_t *));
    // incoming_client_events = xQueueCreate(100, sizeof(client_event_t));

    ret = wifi_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Wifi init ERROR !!!\n");
    }
    ESP_LOGI(TAG, "ESP_WIFI_INIT\n");
    ESP_ERROR_CHECK(ret);

    initialise_mdns();
    server = web_server();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "web server ERROR !!!\n");
    }
    ESP_LOGI(TAG, "DALE\n");
    ESP_ERROR_CHECK(ret);

    // xTaskCreate(&img_stream, "img_stream", 4096, NULL, 1, NULL);
    while (1)
    {
        vTaskDelay(20000);
    }
}
