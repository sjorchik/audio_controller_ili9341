// net/web_api_time.cpp [S2a]: GET /api/time.

#include "net/web_api_time.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <stdio.h>
#include <time.h>

#include "core/time_service.h"

namespace {

// Неблокуючий обробник (виконується в задачі AsyncTCP): лише читання атомарного стану й форматування.
void handleTimeGet(AsyncWebServerRequest* req) {
    time_t epoch = 0;
    tm t;
    const bool valid = TimeService::getNow(epoch, t);

    JsonDocument doc;
    doc["valid"] = valid;
    if (valid) {
        char local[16];
        char date[16];
        snprintf(local, sizeof(local), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
        snprintf(date, sizeof(date), "%04d-%02d-%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
        doc["epoch"] = static_cast<long long>(epoch);
        doc["local"] = local;
        doc["date"] = date;
    } else {
        doc["epoch"] = nullptr;
        doc["local"] = nullptr;
        doc["date"] = nullptr;
    }
    doc["tz"] = TimeService::tzName();
    if (valid) {
        doc["msSinceSync"] = TimeService::msSinceSync();
    } else {
        doc["msSinceSync"] = nullptr;
    }

    AsyncResponseStream* r = req->beginResponseStream("application/json");
    if (r == nullptr) {
        req->send(503);
        return;
    }
    r->addHeader("Cache-Control", "no-store");
    serializeJson(doc, *r);
    req->send(r);
}

}  // namespace

void registerTimeRoutes(AsyncWebServer& server) {
    server.on("/api/time", HTTP_GET, handleTimeGet);
}
