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

void httpd_task(void *pvParameters) {
    tCGI pCGIs[] = {
        {"/index", (tCGIHandler) index_cgi_handler},
        {"/up", (tCGIHandler) up_cgi_handler},
        {"/down", (tCGIHandler) down_cgi_handler},
        {"/stop", (tCGIHandler) stop_cgi_handler},
        {"/set_up_pos", (tCGIHandler) set_up_position_cgi_handler},
        {"/set_down_pos", (tCGIHandler) set_down_position_cgi_handler},
        {"/set_target_position", (tCGIHandler) set_target_position_cgi_handler},
    };

    http_set_cgi_handlers(pCGIs, sizeof(pCGIs) / sizeof(pCGIs[0]));
    httpd_init();

    for(;;) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void http_init(window_cover_t* _cover) {
    cover = _cover;
    printf("Starting http server\n");
    xTaskCreate(&httpd_task, "HTTP Daemon", 200, NULL, 1, NULL);
    printf("HTTP server started\n");
}