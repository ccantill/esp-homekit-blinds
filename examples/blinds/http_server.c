#include "http_server.h"

#include <stdio.h>
#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <httpd/httpd.h>

#include "blinds_controller.h"

window_cover_t *cover;

#define MAX_WEBSOCKET_CLIENTS 10

char ws_status_message[200];
int ws_status_message_len;

TaskHandle_t websocket_tasks[MAX_WEBSOCKET_CLIENTS];

char *index_cgi_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    return "/index.html";
}

char *up_cgi_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    WindowCover_setState(cover, DECREASING);
    return "/ok";
}

char *down_cgi_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    WindowCover_setState(cover, INCREASING);
    return "/ok";
}

char *stop_cgi_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    WindowCover_setState(cover, STOPPED);
    return "/ok";
}

char *set_up_position_cgi_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    WindowCover_setMaxUpPosition(cover);
    return "/ok";
}

char *set_down_position_cgi_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    WindowCover_setMaxDownPosition(cover);
    return "/ok";
}

char *set_target_position_cgi_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    WindowCover_setTargetPosition(cover, strtol(pcValue[0], NULL, 10) * cover->maxPosition / 100);
    return "/ok";
}

void websocket_update_message() {
    /* Generate response in JSON format */
    ws_status_message_len = snprintf(ws_status_message, sizeof (ws_status_message),
            "{\"current_position\": %d, \"target_position\": %d, \"max_position\": %d, \"state\": %d}",
            cover->currentPosition, cover->targetPosition, cover->maxPosition, cover->state);
}

void websocket_task(void *pvParameter)
{
    struct tcp_pcb *pcb = (struct tcp_pcb *) pvParameter;

    for (;;) {
        if (pcb == NULL || pcb->state != ESTABLISHED) {
            printf("Connection closed, deleting task\n");
            break;
        }

        if (ws_status_message_len < sizeof (ws_status_message))
            websocket_write(pcb, (unsigned char *) ws_status_message, ws_status_message_len, WS_TEXT_MODE);

        vTaskSuspend(NULL);
    }

    TaskHandle_t taskHandle = xTaskGetCurrentTaskHandle();

    for(int i=0;i<MAX_WEBSOCKET_CLIENTS;i++) {
        if(websocket_tasks[i] == taskHandle) {
            websocket_tasks[i] = NULL;
            break;
        };
    }
    vTaskDelete(NULL);
}

void websocket_broadcast()
{
    websocket_update_message();
    for(int i=0;i<MAX_WEBSOCKET_CLIENTS;i++) {
        if(websocket_tasks[i] && eTaskGetState(websocket_tasks[i]) == eSuspended) {
            vTaskResume(websocket_tasks[i]);
        };
    }
}

void websocket_cb(struct tcp_pcb *pcb, uint8_t *data, u16_t data_len, uint8_t mode)
{
}

/**
 * This function is called when new websocket is open and
 * creates a new websocket_task if requested URI equals '/stream'.
 */
void websocket_open_cb(struct tcp_pcb *pcb, const char *uri)
{
    printf("WS URI: %s\n", uri);
    
    for(int i=0;i<MAX_WEBSOCKET_CLIENTS;i++) {
        if(!websocket_tasks[i]) {
            xTaskCreate(&websocket_task, "websocket_task", 256, (void *) pcb, 2, &websocket_tasks[i]);
            break;
        };
    }
}

void httpd_task(void *pvParameters) {
    tCGI pCGIs[] = {
        {"/index", (tCGIHandler) index_cgi_handler},
        {"/up", (tCGIHandler) up_cgi_handler},
        {"/down", (tCGIHandler) down_cgi_handler},
        {"/stop", (tCGIHandler) stop_cgi_handler},
        {"/set_up_pos", (tCGIHandler) set_up_position_cgi_handler},
        {"/set_down_pos", (tCGIHandler) set_down_position_cgi_handler},
        {"/set_target_position", (tCGIHandler) set_target_position_cgi_handler}
    };

    http_set_cgi_handlers(pCGIs, sizeof(pCGIs) / sizeof(pCGIs[0]));

    for(int i=0;i<MAX_WEBSOCKET_CLIENTS;i++) {
        websocket_tasks[i] = NULL;
    }
    websocket_update_message();
    websocket_register_callbacks((tWsOpenHandler) websocket_open_cb,
            (tWsHandler) websocket_cb);
    httpd_init();

    for(;;) {
        vTaskDelay(15000 / portTICK_PERIOD_MS);
        websocket_broadcast();
    }
}

void http_init(window_cover_t* _cover) {
    cover = _cover;
    printf("Starting http server\n");
    xTaskCreate(&httpd_task, "HTTP Daemon", 200, NULL, 1, NULL);
    printf("HTTP server started\n");
}