#ifndef __BLINDS_CONTROLLER
#define __BLINDS_CONTROLLER

#include <stdbool.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

typedef enum window_state {
    INCREASING,
    DECREASING,
    STOPPED
} window_state_t;

typedef enum window_cover_event_type {
    TARGET_POSITION_CHANGED,
    CURRENT_POSITION_CHANGED,
    STATE_CHANGED,
    MAX_UP_POSITION_CHANGED,
    MAX_DOWN_POSITION_CHANGED
} window_cover_event_type_t;

struct window_cover;

typedef void (*window_cover_event_handler) (struct window_cover* cover, window_cover_event_type_t);

typedef struct window_cover {
    int maxPosition, currentPosition, targetPosition;
    int sensePin, upPin, downPin;
    bool lastSenseState;
    int index;
    char* prefix;
    char* name;
    char* currentPosition_name;
    char* maxPosition_name;
    window_cover_event_handler callback;

    TaskHandle_t monitorTask;
    
    window_state_t state;
} window_cover_t;

void WindowCover_init(window_cover_t* instance);
void WindowCover_start(window_cover_t* instance);
void WindowCover_setState(window_cover_t* instance, window_state_t state);
window_state_t WindowCover_getState(window_cover_t* instance);
void WindowCover_setTargetPosition(window_cover_t* instance, int newPosition);
int WindowCover_getTargetPosition(window_cover_t* instance);
int WindowCover_getCurrentPosition(window_cover_t* instance);
int WindowCover_getMaxPosition(window_cover_t* instance);
void WindowCover_setMaxUpPosition(window_cover_t* instance);
void WindowCover_setMaxDownPosition(window_cover_t* instance);

#endif