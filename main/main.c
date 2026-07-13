#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#include <pb_encode.h>
#include "gtfs-realtime.pb.h"
#include "wifi_manager.h"



static const char *TAG = "main";

void app_main(void)
{
    wifi_init();

    while (1)
    {


        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
