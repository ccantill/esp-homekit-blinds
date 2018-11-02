#ifndef __BLINDS_CONTROLLER
#define __BLINDS_CONTROLLER

#include <stdbool.h>

typedef enum window_state {
    INCREASING,
    DECREASING,
    STOPPED
} window_state_t;

typedef struct window_cover {
    int maxPosition, currentPosition, targetPosition;
    int sensePin, upPin, downPin;
    bool lastSenseState;
    window_state_t state;
} window_cover_t;

void WindowCover_init(window_cover_t* instance, int sensePin, int upPin, int downPin);
void WindowCover_setState(window_cover_t* instance, window_state_t state);
window_state_t WindowCover_getState(window_cover_t* instance);
void WindowCover_setTargetPosition(window_cover_t* instance, int newPosition);
int WindowCover_getTargetPosition(window_cover_t* instance);
int WindowCover_getCurrentPosition(window_cover_t* instance);
int WindowCover_getMaxPosition(window_cover_t* instance);
void WindowCover_setMaxUpPosition(window_cover_t* instance);
void WindowCover_setMaxDownPosition(window_cover_t* instance);
void WindowCover_update(window_cover_t* instance);

#endif