#include "http_server.h"

#include <stdio.h>

#include <libesphttpd/esp.h>
#include "libesphttpd/httpd.h"
#include "libesphttpd/httpdespfs.h"
#include "libesphttpd/espfs.h"
#include "libesphttpd/webpages-espfs.h"
#include "libesphttpd/cgiwebsocket.h"
#include "libesphttpd/route.h"
#include "libesphttpd/httpd-freertos.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "blinds_controller.h"

#define MAX_CONNECTIONS 32u
#define LISTEN_PORT 80u

window_cover_t *cover;

static char connectionMemory[sizeof(RtosConnType) * MAX_CONNECTIONS];
static HttpdFreertosInstance httpdFreertosInstance;


char ws_status_message[200];
int ws_status_message_len;

// TaskHandle_t websocket_tasks[MAX_WEBSOCKET_CLIENTS];

typedef bool (*simple_callback) (window_cover_t*, HttpdConnData*);

bool up(window_cover_t* cover, HttpdConnData *connData) {
    WindowCover_setState(cover, DECREASING);
    return true;
}

bool down(window_cover_t* cover,HttpdConnData *connData ) {
    WindowCover_setState(cover, INCREASING);
    return true;
}

bool stop(window_cover_t* cover, HttpdConnData *connData) {
    WindowCover_setState(cover, STOPPED);
    return true;
}

bool resetUpPosition(window_cover_t* cover, HttpdConnData *connData) {
    WindowCover_setMaxUpPosition(cover);
    return true;
}

bool resetDownPosition(window_cover_t* cover, HttpdConnData *connData) {
    WindowCover_setMaxDownPosition(cover);
    return true;
}

bool setTargetPosition(window_cover_t* cover, HttpdConnData *connData) {
    int len;
    char buf[4];
    len = httpdFindArg(connData->getArgs, "pos", buf, sizeof(buf));
    if(len!=0) {
        printf("pos=%s\n", buf);
        int newPositionInPercent = atoi(buf);
        WindowCover_setTargetPosition(cover, newPositionInPercent * cover->maxPosition / 100);
        return true;
    } else {
        return false;
    }
}

CgiStatus ICACHE_FLASH_ATTR invoke(HttpdConnData *connData) {
    bool allOk = ((simple_callback)connData->cgiArg)(cover, connData);
    if(allOk) {
        httpdStartResponse(connData, 200);
        httpdEndHeaders(connData);
        httpdSend(connData, "OK", 2);
    } else {
        httpdStartResponse(connData, 500);
        httpdEndHeaders(connData);
        httpdSend(connData, "ERROR", 5);
    }
    return HTTPD_CGI_DONE;
}

void websocket_update_message() {
    /* Generate response in JSON format */
    ws_status_message_len = snprintf(ws_status_message, sizeof (ws_status_message),
            "{\"current_position\": %d, \"target_position\": %d, \"max_position\": %d, \"state\": %d}",
            cover->currentPosition, cover->targetPosition, cover->maxPosition, cover->state);
}

void websocketBroadcastTask() {
    for(;;) {
        websocket_broadcast();
        vTaskDelay(18000/portTICK_RATE_MS);
    }
}

void websocket_broadcast() {
        websocket_update_message();
        cgiWebsockBroadcast(&httpdFreertosInstance.httpdInstance, "/status", ws_status_message, ws_status_message_len, WEBSOCK_FLAG_NONE);
}

static void websocketConnect(Websock *ws) {
    cgiWebsocketSend(&httpdFreertosInstance.httpdInstance, ws, ws_status_message, ws_status_message_len, WEBSOCK_FLAG_NONE);
}

HttpdBuiltInUrl builtInUrls[]={
    ROUTE_FILESYSTEM(),
    ROUTE_CGI_ARG("/up", invoke, up),
    ROUTE_CGI_ARG("/down", invoke, down),
    ROUTE_CGI_ARG("/stop", invoke, stop),
    ROUTE_CGI_ARG("/set_up_pos", invoke, resetUpPosition),
    ROUTE_CGI_ARG("/set_down_pos", invoke, resetDownPosition),
    ROUTE_CGI_ARG("/set_target_position", invoke, setTargetPosition),
    ROUTE_WS("/status", websocketConnect),
    ROUTE_END()
};

void http_init(window_cover_t* _cover) {
    cover = _cover;
    printf("Starting http server\n");

	espFsInit((void*)(webpages_espfs_start));

    websocket_update_message();

    httpdFreertosInit(&httpdFreertosInstance,
	                  builtInUrls,
	                  LISTEN_PORT,
	                  connectionMemory,
	                  MAX_CONNECTIONS,
	                  HTTPD_FLAG_NONE);
	httpdFreertosStart(&httpdFreertosInstance);
}