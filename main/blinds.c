#include <stdio.h>
#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "homekit.h"

#include <wifi_config.h>

#include "blinds_controller.h"
#include "http_server.h"

#define POSITION_STATE_DECREASING 0
#define POSITION_STATE_INCREASING 1
#define POSITION_STATE_STOPPED 2

const int led_gpio = 2;
bool led_on = false;

nvs_handle settings_storage;

homekit_value_t current_position_get(window_cover_t* cover);
homekit_value_t target_position_get(window_cover_t* cover);
homekit_value_t position_state_get(window_cover_t* cover);

void led_write(bool on) {
    gpio_set_level(led_gpio, on ? 1 : 0);
}

void led_init() {
    gpio_set_direction(led_gpio, GPIO_MODE_OUTPUT);
    led_write(led_on);
}

void led_identify_task(void *_args) {
    for (int i=0; i<3; i++) {
        for (int j=0; j<2; j++) {
            led_write(true);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            led_write(false);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }

        vTaskDelay(250 / portTICK_PERIOD_MS);
    }

    led_write(led_on);

    vTaskDelete(NULL);
}

void led_identify(homekit_value_t _value) {
    printf("LED identify\n");
    xTaskCreate(led_identify_task, "LED identify", 512, NULL, 2, NULL);
}

homekit_value_t led_on_get() {
    return HOMEKIT_BOOL(led_on);
}

void led_on_set(homekit_value_t value) {
    if (value.format != homekit_format_bool) {
        printf("Invalid value format: %d\n", value.format);
        return;
    }

    led_on = value.bool_value;
    led_write(led_on);
}

#define MOTOR_START(count) window_cover_t covers[] = {
#define MOTOR(idx, nme, pfx, up_pin, down_pin, sense_pin)\
    (window_cover_t){ .index=idx, .prefix=pfx, .name = nme, .upPin = up_pin, .downPin = down_pin, .sensePin = sense_pin, .maxPosition_name = "max_pos_" pfx, .currentPosition_name = "cur_pos_" pfx },
#define MOTOR_END() };
#include "config.h"
#undef MOTOR_START
#undef MOTOR
#undef MOTOR_END

void homekit_callback(window_cover_t* cover, window_cover_event_type_t type) {
    websocket_broadcast();
    switch(type) {
        case MAX_UP_POSITION_CHANGED:
        case MAX_DOWN_POSITION_CHANGED:
            nvs_set_i32(settings_storage, cover->maxPosition_name, cover->maxPosition);
            // continue on because a change on max position also has an effect on the current position
        case CURRENT_POSITION_CHANGED:
            homekit_characteristic_notify(current_position[cover->index], current_position_get(cover));
            nvs_set_i32(settings_storage, cover->currentPosition_name, cover->currentPosition);
            nvs_commit(settings_storage);
            break;
        case TARGET_POSITION_CHANGED:
            homekit_characteristic_notify(target_position[cover->index], target_position_get(cover));
            break;
        case STATE_CHANGED:
            homekit_characteristic_notify(position_state[cover->index], position_state_get(cover));
    }
}

void blinds_init() {
    esp_err_t err;
    for(int i=0; i < sizeof(covers) / sizeof(window_cover_t); i++) {
        printf("blinds_init idx %d\n", i);
        window_cover_t* cover = &covers[i];
        printf("Initializing motor %s\n", cover->name);
        if(
                (err = nvs_get_i32(settings_storage, cover->currentPosition_name, &cover->currentPosition)) != ESP_OK ||
                (err = nvs_get_i32(settings_storage, cover->maxPosition_name, &cover->maxPosition)) != ESP_OK) {
            printf("Could not fetch previous values: %s", esp_err_to_name(err));
            printf("Initializing with default settings\n");
            ESP_ERROR_CHECK(nvs_set_i32(settings_storage, cover->currentPosition_name, cover->currentPosition = 0));
            ESP_ERROR_CHECK(nvs_set_i32(settings_storage, cover->maxPosition_name, cover->maxPosition = 24));
            ESP_ERROR_CHECK(nvs_commit(settings_storage));
        } else {
            printf("Restored previous settings: C=%d, M=%d\n", cover->currentPosition, cover->maxPosition);
        }
        cover->targetPosition = cover->currentPosition;
        cover->callback = homekit_callback;
        WindowCover_start(cover);
    }
}

homekit_value_t current_position_get(window_cover_t* cover) {
    int cp;
    if(cover->state == STOPPED && cover->targetPosition >= 0) {
        cp = cover->targetPosition;
    } else {
        cp = cover->currentPosition;
    }
    uint8_t mp = cover->maxPosition, pct = cp * 100 / mp;
    printf("Current Position: %d / %d = %d %%\n", cp, mp, pct);
    if(pct > 100) {
        pct = 100;
    }
    return HOMEKIT_UINT8(100 - pct);
}

homekit_value_t current_position_get_i(int index) {
    window_cover_t* cover = &covers[index];
    return current_position_get(cover);
}

homekit_value_t target_position_get(window_cover_t* cover) {
    int cp = cover->targetPosition;
    if(cp < 0) {
        return current_position_get(cover);
    }
    uint8_t mp = cover->maxPosition, pct = cp * 100 / mp;
    printf("Target Position: %d / %d = %d %%\n", cp, mp, pct);
    if(pct > 100) {
        pct = 100;
    }
    return HOMEKIT_UINT8(100 - pct);
}

homekit_value_t target_position_get_i(int index) {
    window_cover_t* cover = &covers[index];
    return target_position_get(cover);
}

void target_position_set_i(int index, homekit_value_t position) {
    window_cover_t* cover = &covers[index];
    WindowCover_setTargetPosition(cover, (100 - position.int_value) * WindowCover_getMaxPosition(cover) / 100);
}

homekit_value_t position_state_get(window_cover_t* cover) {
    return HOMEKIT_UINT8(POSITION_STATE_STOPPED);
}

homekit_value_t position_state_get_i(int index) {
    return position_state_get(&covers[index]);
} 

void on_wifi_ready() {
    led_init();
    blinds_init();
    homekit_server_init(&config);
    http_init(covers, sizeof(covers) / sizeof(window_cover_t));
}

#define NO_WIFI_CONFIG

#ifdef NO_WIFI_CONFIG
#include "wifi.h"
esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
        case SYSTEM_EVENT_STA_START:
            printf("STA start\n");
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            printf("WiFI ready\n");
            on_wifi_ready();
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            printf("STA disconnected\n");
            esp_wifi_connect();
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void start_wifi_config() {
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}
#else
void start_wifi_config() {
    wifi_config_init("blinds", NULL, on_wifi_ready);
}
#endif

void app_main(void) {
    homekit_init();

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );
    
    ret = nvs_open("blinds_settings", NVS_READWRITE, &settings_storage);
    if(ret != ESP_OK) {
        printf("Error (%s) opening NVS handle\n", esp_err_to_name(ret));
    }

    for(int i = 0; i < sizeof(covers) / sizeof(window_cover_t); i++) {
        WindowCover_init(&covers[i]);
    }

    start_wifi_config();
}

