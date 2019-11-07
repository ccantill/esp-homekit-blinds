#include "blinds_controller.h"

#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>


bool WindowCover_update(window_cover_t* instance);

void monitor_task(void *_args) {
    window_cover_t* instance = (window_cover_t*) _args;
    for(;;) {
        WindowCover_update(instance);
        vTaskDelay(80 / portTICK_PERIOD_MS);
    }
}

void WindowCover_init(window_cover_t* instance) {
    printf("Motor intialisation. Sense: %d, up: %d, down: %d\n", instance->sensePin, instance->upPin, instance->downPin);
    gpio_set_direction(instance->upPin, GPIO_MODE_OUTPUT);
    gpio_set_direction(instance->downPin, GPIO_MODE_OUTPUT);

    gpio_set_direction(instance->sensePin, GPIO_MODE_INPUT);
    gpio_set_pull_mode(instance->sensePin, GPIO_PULLDOWN_ONLY);

    gpio_set_level(instance->upPin, 0);
    gpio_set_level(instance->downPin, 0);

    instance->state = STOPPED;
    instance->lastSenseState = gpio_get_level(instance->sensePin);
}

void WindowCover_start(window_cover_t* instance) {
    xTaskCreate(monitor_task, "Monitor", 8000, (void *) instance, 2, &instance->monitorTask);
}

int WindowCover_getCurrentPosition(window_cover_t* instance) {
    return instance->currentPosition;
}

int WindowCover_getMaxPosition(window_cover_t* instance) {
    return instance->maxPosition;
}

window_state_t WindowCover_getState(window_cover_t* instance) {
    return instance->state;
}

void WindowCover_setStateInternal(window_cover_t* instance, window_state_t newState) {
    instance->state = newState;
    gpio_set_level(instance->upPin, newState == DECREASING);
    gpio_set_level(instance->downPin, newState == INCREASING);
    if(instance->callback != NULL) {
        instance->callback(instance, STATE_CHANGED);
    }
}

bool WindowCover_update(window_cover_t* instance) {
    if(instance->state != STOPPED) {   
        bool newSenseState = gpio_get_level(instance->sensePin);
        if(newSenseState != instance->lastSenseState) {
            instance->lastSenseState = newSenseState;
            if(instance->state == DECREASING) {
                instance->currentPosition--;
            } else if(instance->state == INCREASING) {
                instance->currentPosition++;
            }
            if(instance->callback != NULL) {
                instance->callback(instance, CURRENT_POSITION_CHANGED);
            }
            printf("Current position now at %d\n", instance->currentPosition);
        }

        if(instance->targetPosition != -1) {
            if((instance->state == INCREASING && instance->currentPosition >= instance->targetPosition) || (instance->state == DECREASING && instance->currentPosition <= instance->targetPosition)) {
                WindowCover_setStateInternal(instance, STOPPED);
            }
        }
        return true;
    } else {
        return false;
    }
}

void WindowCover_setTargetPosition(window_cover_t* instance, int position) {
    instance->targetPosition = position;
    if(instance->callback != NULL) {
        instance->callback(instance, TARGET_POSITION_CHANGED);
    }
    if(position > instance->currentPosition) {
        WindowCover_setStateInternal(instance, INCREASING);
    } else if(position < instance->currentPosition) {
        WindowCover_setStateInternal(instance, DECREASING);
    }
}

int WindowCover_getTargetPosition(window_cover_t* instance) {
    return instance->targetPosition;
}

void WindowCover_setMaxDownPosition(window_cover_t* instance) {
    instance->maxPosition = instance->currentPosition;
    if(instance->callback != NULL) {
        instance->callback(instance, MAX_DOWN_POSITION_CHANGED);
    }
}

void WindowCover_setMaxUpPosition(window_cover_t* instance) {
    instance->maxPosition -= instance->currentPosition;
    instance->targetPosition -= instance->currentPosition;
    instance->currentPosition = 0;
    if(instance->callback != NULL) {
        instance->callback(instance, MAX_UP_POSITION_CHANGED);
    }
}

void WindowCover_setState(window_cover_t* instance, window_state_t newState) {
    instance->targetPosition = -1;
    if(instance->state != newState) {
        WindowCover_setStateInternal(instance, newState);
    }
}