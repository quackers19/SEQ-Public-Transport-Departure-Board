#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <pb_encode.h>
#include "gtfs-realtime.pb.h"
#include "wifi_manager.h"
#include "schedule_db_manager.h"


static const char *TAG = "main";

void app_main(void)
{
    wifi_init();
    vTaskDelay(5000);

    while (1)
    {       
        vTaskDelay(pdMS_TO_TICKS(10000));
        xTaskCreate(&check_github_artifact_task, "check_gh_artifact", 8192, NULL, 5, NULL);
    }
}
