#include <homekit/homekit.h>
#include <homekit/characteristics.h>

void led_identify(homekit_value_t _value);

// callback generation

homekit_value_t current_position_get_i(int index);
homekit_value_t target_position_get_i(int index);
homekit_value_t position_state_get_i(int index);

void target_position_set_i(int index, homekit_value_t position);

#define MOTOR_START(count)\
    homekit_characteristic_t *current_position[count];\
    homekit_characteristic_t *target_position[count];\
    homekit_characteristic_t *position_state[count];
#define MOTOR(index, name, prefix, up_pin, down_pin, sense_pin)\
    homekit_value_t current_position_get_##index () { return current_position_get_i(index); }\
    homekit_value_t target_position_get_##index () { return target_position_get_i(index); }\
    homekit_value_t position_state_get_##index () { return position_state_get_i(index); }\
    void target_position_set_##index (homekit_value_t value) { target_position_set_i(index, value); }\
    \
    homekit_characteristic_t current_position_##index = HOMEKIT_CHARACTERISTIC_(\
                    CURRENT_POSITION,\
                    0,\
                    .getter=current_position_get_##index\
                );\
    homekit_characteristic_t target_position_##index = HOMEKIT_CHARACTERISTIC_(\
                    TARGET_POSITION,\
                    0,\
                    .getter=target_position_get_##index,\
                    .setter=target_position_set_##index\
                );\
    homekit_characteristic_t position_state_##index = HOMEKIT_CHARACTERISTIC_(\
                    POSITION_STATE,\
                    0,\
                    .getter=position_state_get_##index\
                );
#define MOTOR_END()
#include "config.h"
#undef MOTOR_START
#undef MOTOR
#undef MOTOR_END

#define MOTOR_START(count) \
    homekit_service_t services[] = { \
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){ \
            HOMEKIT_CHARACTERISTIC(NAME, "Blinds"), \
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "ccantill"), \
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "037A2BABF19D"), \
            HOMEKIT_CHARACTERISTIC(MODEL, "Blinds"), \
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1"), \
            HOMEKIT_CHARACTERISTIC(IDENTIFY, led_identify), \
            NULL \
        }),
#define MOTOR(index, name, prefix, up_pin, down_pin, sense_pin)\
        HOMEKIT_SERVICE(WINDOW_COVERING, .primary=true, .characteristics=(homekit_characteristic_t*[]) { \
            HOMEKIT_CHARACTERISTIC(NAME, name), \
            &current_position_##index , \
            &target_position_##index , \
            &position_state_##index , \
            NULL \
        }),
#define MOTOR_END() \
        NULL \
    };
#include "config.h"
#undef MOTOR_START
#undef MOTOR
#undef MOTOR_END

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_window_covering, .services=services),
    NULL
};

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-111"
};

#define MOTOR_START(count) void homekit_init() {
#define MOTOR(index, name, prefix, up_pin, down_pin, sense_pin)\
        current_position[index] = &current_position_##index;\
        target_position[index] = &target_position_##index; \
        position_state[index] = &position_state_##index;
#define MOTOR_END() };
#include "config.h"
#undef MOTOR_START
#undef MOTOR
#undef MOTOR_END