#include <homekit/homekit.h>
#include <homekit/characteristics.h>

void led_identify(homekit_value_t _value);

homekit_value_t current_position_get();
homekit_value_t target_position_get();
homekit_value_t position_state_get();

void target_position_set(homekit_value_t position);

homekit_characteristic_t current_position = HOMEKIT_CHARACTERISTIC_(
                CURRENT_POSITION,
                0,
                .getter=current_position_get
            );
homekit_characteristic_t target_position = HOMEKIT_CHARACTERISTIC_(
                TARGET_POSITION,
                0,
                .getter=target_position_get,
                .setter=target_position_set
            );
homekit_characteristic_t position_state = HOMEKIT_CHARACTERISTIC_(
                POSITION_STATE,
                0,
                .getter=position_state_get
            );

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
            &current_position,
            &target_position,
            &position_state,
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