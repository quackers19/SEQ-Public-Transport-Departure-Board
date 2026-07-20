#include "schedule_db_manager.h"
#include "sdkconfig.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "cJSON.h"

static bool db_downloaded = false;

#define HTTP_RESPONSE_BUF_SIZE 4096
static char response_buf[HTTP_RESPONSE_BUF_SIZE];
static int response_len = 0;
static const char *TAG = "dbfetch";
static char file_name[] = "gtfs-" CONFIG_STOPS ".db";


static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client)) {
                if (response_len + evt->data_len < HTTP_RESPONSE_BUF_SIZE) {
                    memcpy(response_buf + response_len, evt->data, evt->data_len);
                    response_len += evt->data_len;
                    response_buf[response_len] = '\0';
                } else {
                    ESP_LOGW(TAG, "Response buffer full, truncating");
                }
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH, total_len=%d", response_len);
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        default:
            break;
    }
    return ESP_OK;
}

static bool json_contains_artifact(const char *json_str, const char *target_name)
{
    bool found = false;
    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) {
        const char *err = cJSON_GetErrorPtr();
        ESP_LOGE(TAG, "JSON parse error near: %s", err ? err : "unknown");
        return false;
    }

    cJSON *artifacts = cJSON_GetObjectItem(root, "artifacts");
    if (cJSON_IsArray(artifacts)) {
        cJSON *artifact;
        cJSON_ArrayForEach(artifact, artifacts) {
            cJSON *name = cJSON_GetObjectItem(artifact, "name");
            if (cJSON_IsString(name) && name->valuestring != NULL) {
                ESP_LOGI(TAG, "Found artifact: %s", name->valuestring);
                if (strcmp(name->valuestring, target_name) == 0) {
                    found = true;
                }
            }
        }
    } else {
        ESP_LOGW(TAG, "No 'artifacts' array in response");
    }

    cJSON_Delete(root);
    return found;
}

void check_github_artifact_task(void *pvParameters)
{
    char url[256];
    snprintf(url, sizeof(url),
             "https://api.github.com/repos/%s/%s/actions/artifacts",
             CONFIG_GITHUB_OWNER, CONFIG_GITHUB_REPO);

    response_len = 0;
    response_buf[0] = '\0';

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 8000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "User-Agent", "esp32-artifact-checker");
    esp_http_client_set_header(client, "Accept", "application/vnd.github+json");

    if (strlen(CONFIG_GITHUB_TOKEN) > 0) {
        char auth_header[160];
        snprintf(auth_header, sizeof(auth_header), "Bearer %s", CONFIG_GITHUB_TOKEN);
        esp_http_client_set_header(client, "Authorization", auth_header);
    } else {
        ESP_LOGW(TAG, "No GitHub token configured — using unauthenticated request (60/hr limit)");
    }

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %"PRId64,
                 status, esp_http_client_get_content_length(client));

        if (status == 200) {
            bool exists = json_contains_artifact(response_buf, file_name);
            if (exists) {
                ESP_LOGI(TAG, "Artifact '%s' EXISTS", file_name);
            } else {
                ESP_LOGI(TAG, "Artifact '%s' NOT FOUND", file_name);
            }
        } else {
            ESP_LOGE(TAG, "Unexpected status code: %d, body: %s", status, response_buf);
        }
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    vTaskDelete(NULL);
}