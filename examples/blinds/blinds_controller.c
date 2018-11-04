#include "blinds_controller.h"

#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>

bool WindowCover_update(window_cover_t* instance);

void monitor_task(void *_args) {
    window_cover_t* instance = (window_cover_t*) _args;
    while(true) {
        WindowCover_update(instance);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void WindowCover_init(window_cover_t* instance, int sensePin, int upPin, int downPin) {
    instance->sensePin = sensePin;
    instance->upPin = upPin;
    instance->downPin = downPin;

    gpio_enable(sensePin, GPIO_INPUT);
    gpio_enable(upPin, GPIO_OUTPUT);
    gpio_enable(downPin, GPIO_OUTPUT);

    gpio_write(upPin, 0);
    gpio_write(downPin, 0);

    instance->state = STOPPED;
    instance->lastSenseState = gpio_read(sensePin);
}

void WindowCover_start(window_cover_t* instance) {
    xTaskCreate(monitor_task, "Monitor", 300, (void *) instance, 2, &instance->monitorTask);
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
    gpio_write(instance->upPin, newState == DECREASING);
    gpio_write(instance->downPin, newState == INCREASING);
    if(instance->callback != NULL) {
        instance->callback(STATE_CHANGED);
    }
}

bool WindowCover_update(window_cover_t* instance) {
    if(instance->state != STOPPED) {   
        bool newSenseState = gpio_read(instance->sensePin);
        if(newSenseState != instance->lastSenseState) {
            instance->lastSenseState = newSenseState;
            if(instance->state == DECREASING) {
                instance->currentPosition--;
            } else if(instance->state == INCREASING) {
                instance->currentPosition++;
            }
            if(instance->callback != NULL) {
                instance->callback(CURRENT_POSITION_CHANGED);
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
        instance->callback(TARGET_POSITION_CHANGED);
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
        instance->callback(MAX_DOWN_POSITION_CHANGED);
    }
}

void WindowCover_setMaxUpPosition(window_cover_t* instance) {
    instance->maxPosition -= instance->currentPosition;
    instance->targetPosition -= instance->currentPosition;
    instance->currentPosition = 0;
    if(instance->callback != NULL) {
        instance->callback(MAX_UP_POSITION_CHANGED);
    }
}

void WindowCover_setState(window_cover_t* instance, window_state_t newState) {
    instance->targetPosition = -1;
    if(instance->state != newState) {
        WindowCover_setStateInternal(instance, newState);
    }
}