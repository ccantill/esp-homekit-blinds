#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>

#include "blinds_controller.h"
#include "http_server.h"

#define POSITION_STATE_DECREASING 0
#define POSITION_STATE_INCREASING 1
#define POSITION_STATE_STOPPED 2

const int motor_a_up = 16;
const int motor_a_down = 5;
const int motor_a_sense = 4;

const int led_gpio = 2;
bool led_on = false;

void led_write(bool on) {
    gpio_write(led_gpio, on ? 0 : 1);
}

void led_init() {
    gpio_enable(led_gpio, GPIO_OUTPUT);
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
    xTaskCreate(led_identify_task, "LED identify", 128, NULL, 2, NULL);
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

window_cover_t coverA;

void monitor_task(void *_args) {
    while(true) {
        WindowCover_update(&coverA);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void blinds_init() {
    coverA.currentPosition = 0;
    coverA.maxPosition = 24;
    coverA.targetPosition = 0;
    WindowCover_init(&coverA, motor_a_sense, motor_a_up, motor_a_down);
    xTaskCreate(monitor_task, "Monitor", 200, NULL, 2, NULL);
}

homekit_value_t current_position_get() {
    return HOMEKIT_UINT8(WindowCover_getCurrentPosition(&coverA) * 100 / WindowCover_getMaxPosition(&coverA));
}

homekit_value_t target_position_get() {
    return HOMEKIT_UINT8(WindowCover_getTargetPosition(&coverA) * 100 / WindowCover_getMaxPosition(&coverA));
}

void target_position_set(homekit_value_t position) {
    WindowCover_setTargetPosition(&coverA, position.int_value * WindowCover_getMaxPosition(&coverA) / 100);
}

homekit_value_t position_state_get() {
    return HOMEKIT_UINT8(POSITION_STATE_STOPPED);
} 

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_window_covering, .services=(homekit_service_t*[]){
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Blinds"),
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "ccantill"),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "037A2BABF19D"),
            HOMEKIT_CHARACTERISTIC(MODEL, "Blinds"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, led_identify),
            NULL
        }),
        HOMEKIT_SERVICE(WINDOW_COVERING, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Blinds A"),
            HOMEKIT_CHARACTERISTIC(
                CURRENT_POSITION,
                0,
                .getter=current_position_get
            ),
            HOMEKIT_CHARACTERISTIC(
                TARGET_POSITION,
                0,
                .getter=target_position_get,
                .setter=target_position_set
            ),
            HOMEKIT_CHARACTERISTIC(
                POSITION_STATE,
                0,
                .getter=position_state_get
            ),
            NULL
        }),
        NULL
    }),
    NULL
};

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-111"
};

void on_wifi_ready() {
    led_init();
    blinds_init();
    http_init(&coverA);
    homekit_server_init(&config);
}

#define NO_WIFI_CONFIG

#ifdef NO_WIFI_CONFIG
#include "wifi.h"
static void wifi_init() {
    struct sdk_station_config wifi_config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASSWORD,
    };

    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&wifi_config);
    sdk_wifi_station_connect();
}

void user_init(void) {
    uart_set_baud(0, 115200);
    wifi_init();
    on_wifi_ready();
}
#else
void user_init(void) {
    uart_set_baud(0, 115200);

    wifi_config_init("blinds", NULL, on_wifi_ready);
}
#endif

