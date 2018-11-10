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

static char connectionMemory[sizeof(RtosConnType) * MAX_CONNECTIONS];
static HttpdFreertosInstance httpdFreertosInstance;

window_cover_t *http_covers;
uint http_cover_count;

char ws_status_message[800];
int ws_status_message_len;

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
        int pos = atoi(buf);
        WindowCover_setTargetPosition(cover, pos);
        return true;
    } else {
        return false;
    }
}

CgiStatus ICACHE_FLASH_ATTR invoke(HttpdConnData *connData) {
    int idx = (int) connData->cgiArg2;
    bool allOk = ((simple_callback)connData->cgiArg)(&http_covers[idx], connData);
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

CgiStatus ICACHE_FLASH_ATTR status(HttpdConnData *connData) {
    httpdStartResponse(connData, 200);
    httpdHeader(connData, "content-type", "application/json");
    httpdEndHeaders(connData);
    httpdSend(connData, ws_status_message, ws_status_message_len);
    return HTTPD_CGI_DONE;
}

void websocket_update_message() {
    /* Generate response in JSON format */
    uint offset=0;
    offset += snprintf(ws_status_message + offset, sizeof(ws_status_message) - offset, "{");
    for(int i = 0; i < http_cover_count;i++) {
        window_cover_t* cover = &http_covers[i];
        if(i > 0) {
            offset += snprintf(ws_status_message + offset, sizeof(ws_status_message) - offset, ", ");
        }
        offset += snprintf(ws_status_message + offset, sizeof (ws_status_message) - offset,
                "\"%s\": {\"name\": \"%s\", \"current_position\": %d, \"target_position\": %d, \"max_position\": %d, \"state\": %d}",
                cover->prefix, cover->name, cover->currentPosition, cover->targetPosition, cover->maxPosition, cover->state);
    }
    ws_status_message_len = offset + snprintf(ws_status_message + offset, sizeof(ws_status_message) - offset, "}");
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

#define MOTOR_START(count) \
    HttpdBuiltInUrl builtInUrls[]={ \
        ROUTE_FILESYSTEM(),
#define MOTOR(idx, nme, pfx, up_pin, down_pin, sense_pin) \
        ROUTE_CGI_ARG2("/" pfx "/up", invoke, up, idx), \
        ROUTE_CGI_ARG2("/" pfx "/down", invoke, down, idx), \
        ROUTE_CGI_ARG2("/" pfx "/stop", invoke, stop, idx), \
        ROUTE_CGI_ARG2("/" pfx "/set_up_pos", invoke, resetUpPosition, idx), \
        ROUTE_CGI_ARG2("/" pfx "/set_down_pos", invoke, resetDownPosition, idx), \
        ROUTE_CGI_ARG2("/" pfx "/set_target_position", invoke, setTargetPosition, idx),
#define MOTOR_END() \
        ROUTE_CGI("/status", status), \
        ROUTE_WS("/status_ws", websocketConnect), \
        ROUTE_END() \
    };
#include "config.h"
#undef MOTOR_START
#undef MOTOR
#undef MOTOR_END

void http_init(window_cover_t *_covers, uint _cover_count) {
    http_covers = _covers;
    http_cover_count = _cover_count;
    
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