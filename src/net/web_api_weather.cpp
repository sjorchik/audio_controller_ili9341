// net/web_api_weather.cpp [S2b]: /api/weather*.
//
// Усі обробники виконуються в задачі async_tcp і НЕ блокують: читають / змінюють стан WeatherService
// у RAM (м'ютекс на мікросекунди); мережа й запис у NVS — у задачі "weather".
// Формат помилок і хелпери — як в решті API (net/web_api_common.h).
//
// ВАЖЛИВО: ESPAsyncWebServer зіставляє "/api/weather" і з "/api/weather/...", тому маршрути з довшим
// шляхом реєструються РАНІШЕ, а кожен обробник перевіряє точний URL (web_api::exactUrl).

#include "net/web_api_weather.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <string.h>

#include "config/weather_config.h"
#include "core/weather_codes.h"
#include "core/weather_service.h"
#include "core/weather_util.h"
#include "net/web_api_common.h"

namespace {

using web_api::exactUrl;
using web_api::readJsonObjectBody;
using web_api::sendError;
using web_api::sendFieldError;
using web_api::sendJson;
using web_api::sendRangeError;

const char* phaseName(WeatherService::SearchPhase p) {
    switch (p) {
        case WeatherService::SearchPhase::Idle:      return "idle";
        case WeatherService::SearchPhase::Searching: return "searching";
        case WeatherService::SearchPhase::Done:      return "done";
        case WeatherService::SearchPhase::NotFound:  return "not_found";
        case WeatherService::SearchPhase::Error:     return "error";
    }
    return "idle";
}

// --- GET /api/weather ------------------------------------------------------------------------------
void handleWeatherGet(AsyncWebServerRequest* req) {
    if (!exactUrl(req, "/api/weather")) return;

    WeatherService::Status st;
    if (!WeatherService::getStatus(st)) {
        sendError(req, 503, "busy");
        return;
    }

    // st живе до кінця обробника: ArduinoJson лише посилається на char-масиви (sendJson синхронний).
    JsonDocument doc;
    doc["configured"] = st.loc.set;
    if (st.loc.set) {
        doc["city"] = st.loc.name;
        doc["lat"] = weather_util::round4d(st.loc.lat);
        doc["lon"] = weather_util::round4d(st.loc.lon);
    } else {
        doc["city"] = nullptr;
        doc["lat"] = nullptr;
        doc["lon"] = nullptr;
    }
    doc["valid"] = st.valid;
    if (st.valid) {
        doc["tempC"] = weather_util::round1d(st.cur.tempC);
        doc["code"] = st.cur.code;
        doc["isDay"] = st.cur.isDay;
        doc["windMs"] = weather_util::round1d(st.cur.windMs);
        doc["humidity"] = st.cur.humidity;
        if (st.cur.pressureHpa > 0) {  // [S2b3] 0 = немає даних -> null
            doc["pressureHpa"] = st.cur.pressureHpa;
        } else {
            doc["pressureHpa"] = nullptr;
        }
        if (st.cur.windDirDeg >= 0) {  // [S2b3] -1 = немає даних -> null
            doc["windDirDeg"] = st.cur.windDirDeg;
        } else {
            doc["windDirDeg"] = nullptr;
        }
        doc["condition"] = weather::conditionName(weather::category(st.cur.code, st.cur.isDay));
        doc["ageMs"] = st.ageMs;
    } else {
        doc["tempC"] = nullptr;
        doc["code"] = nullptr;
        doc["isDay"] = nullptr;
        doc["windMs"] = nullptr;
        doc["humidity"] = nullptr;
        doc["pressureHpa"] = nullptr;  // [S2b3]
        doc["windDirDeg"] = nullptr;   // [S2b3]
        doc["condition"] = nullptr;
        doc["ageMs"] = nullptr;
    }
    if (st.error[0] != '\0') {
        doc["error"] = st.error;
    } else {
        doc["error"] = nullptr;
    }
    sendJson(req, 200, doc);
}

// --- POST /api/weather/search ----------------------------------------------------------------------
void handleSearchPost(AsyncWebServerRequest* req) {
    if (!exactUrl(req, "/api/weather/search")) return;

    JsonDocument in;
    if (!readJsonObjectBody(req, in)) return;
    JsonObjectConst root = in.as<JsonObjectConst>();
    for (JsonPairConst kv : root) {
        if (strcmp(kv.key().c_str(), "query") != 0) {
            sendFieldError(req, 400, "unknown_field", kv.key().c_str());
            return;
        }
    }
    JsonVariantConst q = root["query"];
    if (q.isNull()) {
        sendFieldError(req, 400, "missing_field", "query");
        return;
    }
    if (!q.is<const char*>()) {
        sendFieldError(req, 400, "invalid_value", "query");
        return;
    }

    switch (WeatherService::requestSearch(q.as<const char*>())) {
        case WeatherService::SearchRequest::Accepted: {
            JsonDocument doc;
            doc["ok"] = true;
            sendJson(req, 202, doc);
            return;
        }
        case WeatherService::SearchRequest::Busy:
            sendError(req, 409, "busy");
            return;
        case WeatherService::SearchRequest::Empty:
        case WeatherService::SearchRequest::BadChars:
            sendFieldError(req, 400, "invalid_value", "query");
            return;
        case WeatherService::SearchRequest::TooLong:
            sendRangeError(req, "query", 1, static_cast<int>(weather_cfg::kQueryMaxBytes));
            return;
        case WeatherService::SearchRequest::Unavailable:
            sendError(req, 503, "busy");
            return;
    }
    sendError(req, 500, "internal");
}

// --- GET /api/weather/search -----------------------------------------------------------------------
void handleSearchGet(AsyncWebServerRequest* req) {
    if (!exactUrl(req, "/api/weather/search")) return;

    WeatherService::SearchState ss;
    if (!WeatherService::getSearchState(ss)) {
        sendError(req, 503, "busy");
        return;
    }

    JsonDocument doc;
    doc["state"] = phaseName(ss.phase);
    if (ss.phase == WeatherService::SearchPhase::Idle) {
        doc["query"] = nullptr;
    } else {
        doc["query"] = ss.query;
    }
    JsonArray results = doc["results"].to<JsonArray>();
    if (ss.phase == WeatherService::SearchPhase::Done) {
        for (uint8_t i = 0; i < ss.count && i < weather_cfg::kMaxCities; ++i) {
            const WeatherService::Candidate& c = ss.results[i];
            JsonObject o = results.add<JsonObject>();
            o["index"] = i;
            o["name"] = c.name;
            if (c.admin1[0] != '\0') {
                o["admin1"] = c.admin1;
            } else {
                o["admin1"] = nullptr;
            }
            if (c.country[0] != '\0') {
                o["country"] = c.country;
            } else {
                o["country"] = nullptr;
            }
            o["lat"] = weather_util::round4d(c.lat);
            o["lon"] = weather_util::round4d(c.lon);
        }
    }
    if (ss.phase == WeatherService::SearchPhase::Error && ss.error[0] != '\0') {
        doc["error"] = ss.error;
    } else {
        doc["error"] = nullptr;
    }
    sendJson(req, 200, doc);
}

// --- POST /api/weather/location --------------------------------------------------------------------
void sendLocationOk(AsyncWebServerRequest* req, const char* city) {
    JsonDocument doc;
    doc["ok"] = true;
    doc["city"] = city;
    sendJson(req, 200, doc);
}

void handleLocationPost(AsyncWebServerRequest* req) {
    if (!exactUrl(req, "/api/weather/location")) return;

    JsonDocument in;
    if (!readJsonObjectBody(req, in)) return;
    JsonObjectConst root = in.as<JsonObjectConst>();
    for (JsonPairConst kv : root) {
        const char* k = kv.key().c_str();
        if (strcmp(k, "index") != 0 && strcmp(k, "name") != 0 && strcmp(k, "lat") != 0 &&
            strcmp(k, "lon") != 0) {
            sendFieldError(req, 400, "unknown_field", k);
            return;
        }
    }

    const bool hasIndex = !root["index"].isNull();
    const bool hasName = !root["name"].isNull();
    const bool hasLat = !root["lat"].isNull();
    const bool hasLon = !root["lon"].isNull();

    if (hasIndex && (hasName || hasLat || hasLon)) {
        sendFieldError(req, 400, "conflicting_fields", "index");
        return;
    }

    // --- Режим 1: кандидат останнього пошуку ---
    if (hasIndex) {
        if (!root["index"].is<int>()) {
            sendFieldError(req, 400, "invalid_value", "index");
            return;
        }
        const int idx = root["index"].as<int>();

        WeatherService::SearchState ss;
        if (!WeatherService::getSearchState(ss)) {
            sendError(req, 503, "busy");
            return;
        }
        if (ss.phase == WeatherService::SearchPhase::Searching) {
            sendError(req, 409, "busy");
            return;
        }
        if (ss.phase != WeatherService::SearchPhase::Done || ss.count == 0) {
            sendError(req, 409, "no_results");
            return;
        }
        if (idx < 0 || idx >= static_cast<int>(ss.count)) {
            sendRangeError(req, "index", 0, static_cast<int>(ss.count) - 1);
            return;
        }
        if (!WeatherService::selectCandidate(idx)) {
            sendError(req, 409, "no_results");  // гонка: з'явився новий пошук між перевірками
            return;
        }
        sendLocationOk(req, ss.results[idx].name);
        return;
    }

    // --- Режим 2: пряме задання ---
    if (!hasName) {
        sendFieldError(req, 400, "missing_field", "name");
        return;
    }
    if (!hasLat) {
        sendFieldError(req, 400, "missing_field", "lat");
        return;
    }
    if (!hasLon) {
        sendFieldError(req, 400, "missing_field", "lon");
        return;
    }
    if (!root["name"].is<const char*>()) {
        sendFieldError(req, 400, "invalid_value", "name");
        return;
    }
    if (!root["lat"].is<float>()) {
        sendFieldError(req, 400, "invalid_value", "lat");
        return;
    }
    if (!root["lon"].is<float>()) {
        sendFieldError(req, 400, "invalid_value", "lon");
        return;
    }
    const char* name = root["name"].as<const char*>();
    const float lat = root["lat"].as<float>();
    const float lon = root["lon"].as<float>();

    // 1..31 байт UTF-8 без керувальних символів (як inputNames у /api/settings).
    if (!weather_util::isTextOk(name, weather_cfg::kNameBytes - 1)) {
        sendFieldError(req, 400, "invalid_value", "name");
        return;
    }
    if (!weather_util::inRange(lat, -90.0f, 90.0f)) {
        sendRangeError(req, "lat", -90, 90);
        return;
    }
    if (!weather_util::inRange(lon, -180.0f, 180.0f)) {
        sendRangeError(req, "lon", -180, 180);
        return;
    }
    if (!WeatherService::setLocation(name, lat, lon)) {
        sendError(req, 503, "busy");
        return;
    }
    sendLocationOk(req, name);
}

// --- DELETE /api/weather/location ------------------------------------------------------------------
void handleLocationDelete(AsyncWebServerRequest* req) {
    if (!exactUrl(req, "/api/weather/location")) return;
    WeatherService::clearLocation();
    JsonDocument doc;
    doc["ok"] = true;
    sendJson(req, 200, doc);
}

}  // namespace

void registerWeatherRoutes(AsyncWebServer& server) {
    // Порядок важливий (див. шапку файлу): довші шляхи — першими.
    server.on("/api/weather/search", HTTP_POST, handleSearchPost, nullptr, web_api::jsonBodyCallback);
    server.on("/api/weather/search", HTTP_GET, handleSearchGet);
    server.on("/api/weather/location", HTTP_POST, handleLocationPost, nullptr, web_api::jsonBodyCallback);
    server.on("/api/weather/location", HTTP_DELETE, handleLocationDelete);
    server.on("/api/weather", HTTP_GET, handleWeatherGet);
}
