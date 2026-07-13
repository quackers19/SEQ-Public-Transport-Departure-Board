#include "wifi_manager.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_netif_ip_addr.h"
#include "esp_timer.h"

// Include the auto-generated config header from menuconfig
#include "sdkconfig.h"

static const char *TAG = "WIFI";

static bool wifi_connected = false;
static bool wifi_connecting = false;

#define WIFI_MAXIMUM_RETRY 10

#define WIFI_RETRY_DELAY_THRESHOLD 5

static int s_retry_num = 0;
static esp_timer_handle_t s_retry_timer = NULL;

// Called by the timer, on a separate timer task, once the delay has elapsed
static void retry_timer_callback(void *arg)
{
    ESP_LOGI(TAG, "Retry delay elapsed, attempting to reconnect...");
    esp_wifi_connect();
}

static void wifi_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch(event_id)
        {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi started, connecting...");
                wifi_connecting = true;
                esp_wifi_connect();
                break;

            case WIFI_EVENT_STA_DISCONNECTED:
                wifi_connected = false;

                if (s_retry_num < WIFI_MAXIMUM_RETRY)
                {
                    wifi_connecting = true;
                    s_retry_num++;

                    if (s_retry_num >= WIFI_RETRY_DELAY_THRESHOLD)
                    {
                        int delay_ms;
                        if (s_retry_num == WIFI_MAXIMUM_RETRY)
                        {
                            delay_ms = 120000; // 2 minutes
                        }
                        else
                        {
                            
                            delay_ms = (s_retry_num - WIFI_RETRY_DELAY_THRESHOLD + 1) * 4000;
                        }

                        ESP_LOGW(TAG, "Disconnected from WiFi, retrying (%d/%d) in %d ms...",
                                 s_retry_num, WIFI_MAXIMUM_RETRY, delay_ms);

                        esp_timer_start_once(s_retry_timer, delay_ms * 1000ULL);
                    }
                    else
                    {
                        ESP_LOGW(TAG, "Disconnected from WiFi, retrying (%d/%d)...",
                                 s_retry_num, WIFI_MAXIMUM_RETRY);
                        esp_wifi_connect();
                    }
                }
                else
                {
                    ESP_LOGE(TAG, "Disconnected from WiFi, max retries reached, giving up");
                    wifi_connecting = false;
                }
                break;
        }
    }

    if (event_base == IP_EVENT &&
        event_id == IP_EVENT_STA_GOT_IP)
    {
        wifi_connected = true;
        wifi_connecting = false;
        s_retry_num = 0; 
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "Connected! IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

void wifi_init(void)
{
    // Initialize NVS where wifi ssid and password are stored
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(
        esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();


    const esp_timer_create_args_t retry_timer_args = {
        .callback = &retry_timer_callback,
        .arg = NULL,
        .name = "wifi_retry_timer",
    };

    ESP_ERROR_CHECK(esp_timer_create(&retry_timer_args, &s_retry_timer));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(
        esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &wifi_event_handler,
            NULL,
            NULL));

    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            &wifi_event_handler,
            NULL,
            NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    strcpy((char *)wifi_config.sta.ssid, CONFIG_WIFI_SSID);
    strcpy((char *)wifi_config.sta.password, CONFIG_WIFI_PASSWORD);

    ESP_ERROR_CHECK(
        esp_wifi_set_mode(WIFI_MODE_STA));

    ESP_ERROR_CHECK(
        esp_wifi_set_config(
            WIFI_IF_STA,
            &wifi_config));

    ESP_ERROR_CHECK(
        esp_wifi_start());
}

bool wifi_is_connected(void)
{
    return wifi_connected;
}

void wifi_check_connection_manual(void)
{
    if (!wifi_connected && !wifi_connecting)
    {
        ESP_LOGI(TAG, "Not connected, attempting to connect...");
        wifi_connecting = true;
        s_retry_num = 0;
        esp_wifi_connect();
    }
}