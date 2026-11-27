// core/weather_service.cpp [S2b]: сервіс погоди й пошуку міста (Open-Meteo).
//
// Потоки й ядра:
//  - усі мережеві запити (TLS) і запис у NVS виконує ЛИШЕ задача "weather" (ядро 0, пріоритет 1);
//  - API-методи (з async_tcp, дисплея, ...) беруть м'ютекс на кілька мікросекунд і повертаються:
//    пошук ставиться в чергу, зміна місця лише міняє стан у RAM й піднімає прапорець s_locDirty,
//    який задача підхоплює за ≤ kPollMs (NVS-запис + негайне оновлення погоди);
//  - м'ютекс НІКОЛИ не тримається під час мережевих викликів (лише на копіювання даних).

#include "core/weather_service.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <Preferences.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <atomic>

#include "core/time_service.h"
#include "core/weather_util.h"
#include "net/wifi_manager.h"

#if WEATHER_DEBUG
#define WX_LOG(fmt, ...) Serial.printf("[WX] " fmt "\n", ##__VA_ARGS__)
#else
#define WX_LOG(...) ((void)0)
#endif

// Вбудований кореневий бандл сертифікатів (символи генерує збірка Arduino-ESP32 / ESP-IDF mbedTLS).
// Якщо лінкер скаржиться на _binary_x509_crt_bundle_start — див. weather_cfg::kTlsInsecure.
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t rootca_crt_bundle_end[] asm("_binary_x509_crt_bundle_end");

namespace {

namespace cfg = weather_cfg;

// --- Спільний стан (захищений s_mutex, якщо не сказано інше) ----------------------------------
SemaphoreHandle_t s_mutex = nullptr;
QueueHandle_t s_queue = nullptr;
TaskHandle_t s_task = nullptr;
bool s_started = false;

WeatherService::Location s_loc = {false, {0}, 0.0f, 0.0f};
uint32_t s_locGen = 0;  // зростає при кожній зміні / очищенні місця: відкидає запізнілі відповіді

bool s_hasData = false;
WeatherService::Current s_cur = {0.0f, 0, true, 0.0f, 0, 0};
char s_error[cfg::kErrorBytes] = {0};

WeatherService::SearchState s_search = {};

// Подвійний буфер назви для cityName() (запис лише під м'ютексом).
char s_nameBuf[2][cfg::kNameBytes] = {{0}, {0}};
std::atomic<uint8_t> s_nameIdx{0};

// Дзеркала для швидкого читання без м'ютекса.
std::atomic<bool> s_locSetFlag{false};
std::atomic<bool> s_hasDataFlag{false};

// Зміна місця: задача має записати NVS й оновити погоду.
std::atomic<bool> s_locDirty{false};

// Буфер відповіді (PSRAM), лише задача "weather".
char* s_resp = nullptr;

// Команди в черзі (лише пошук; зміну місця передає прапорець).
struct Cmd {
    char query[cfg::kQueryMaxBytes + 1];
};

// --- NVS-blob місця ---------------------------------------------------------------------------
struct LocBlob {
    uint8_t version;
    uint8_t flags;       // біт 0 = місце задано
    uint8_t reserved[2];
    float lat;
    float lon;
    char name[cfg::kNameBytes];
};
static_assert(sizeof(LocBlob) == 44, "LocBlob layout changed: bump weather_cfg::kNvsBlobVersion");

// --- М'ютекс ---------------------------------------------------------------------------------
class Lock {
public:
    Lock() : ok_(s_mutex != nullptr && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(cfg::kLockWaitMs)) == pdTRUE) {}
    ~Lock() {
        if (ok_) xSemaphoreGive(s_mutex);
    }
    bool ok() const { return ok_; }
    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;

private:
    bool ok_;
};

void setErrorLocked(const char* code) {
    weather_util::copyUtf8Trunc(s_error, sizeof(s_error), code);
}

void setError(const char* code) {
    Lock lk;
    if (lk.ok()) setErrorLocked(code);
}

// Публікує назву міста для cityName() (під м'ютексом).
void publishNameLocked() {
    const uint8_t next = static_cast<uint8_t>(s_nameIdx.load() ^ 1u);
    weather_util::copyUtf8Trunc(s_nameBuf[next], sizeof(s_nameBuf[next]), s_loc.set ? s_loc.name : "");
    s_nameIdx.store(next);
}

bool validLat(float v) { return weather_util::inRange(v, -90.0f, 90.0f); }
bool validLon(float v) { return weather_util::inRange(v, -180.0f, 180.0f); }

// Застосовує нове місце в RAM (бере м'ютекс сам, тож викликати без нього). NVS пише задача.
bool applyLocation(const char* name, float lat, float lon) {
    Lock lk;
    if (!lk.ok()) return false;
    s_loc.set = true;
    weather_util::copyUtf8Trunc(s_loc.name, sizeof(s_loc.name), name);
    s_loc.lat = lat;
    s_loc.lon = lon;
    ++s_locGen;
    s_hasData = false;
    s_hasDataFlag.store(false);
    s_error[0] = '\0';
    s_locSetFlag.store(true);
    publishNameLocked();
    s_locDirty.store(true);
    return true;
}

// --- NVS ---------------------------------------------------------------------------------------
void loadLocationFromNvs() {
    Preferences prefs;
    // Відкриваємо на запис: у режимі read-only відсутній namespace дає помилку на першому запуску.
    if (!prefs.begin(cfg::kNvsNamespace, false)) {
        WX_LOG("NVS open failed, location not loaded");
        return;
    }
    LocBlob b;
    const size_t len = prefs.getBytesLength(cfg::kNvsKey);
    const bool sizeOk = (len == sizeof(b));
    if (sizeOk) prefs.getBytes(cfg::kNvsKey, &b, sizeof(b));
    prefs.end();

    if (len == 0) {
        WX_LOG("no saved location");
        return;
    }
    if (!sizeOk) {
        WX_LOG("saved location ignored: bad size %u", static_cast<unsigned>(len));
        return;
    }
    if (b.version != cfg::kNvsBlobVersion || (b.flags & 1u) == 0) {
        WX_LOG("saved location ignored: version=%u flags=%u", static_cast<unsigned>(b.version),
               static_cast<unsigned>(b.flags));
        return;
    }
    b.name[sizeof(b.name) - 1] = '\0';
    if (!weather_util::isTextOk(b.name, cfg::kNameBytes - 1) || !validLat(b.lat) || !validLon(b.lon)) {
        WX_LOG("saved location ignored: invalid contents");
        return;
    }
    // begin() ще не запустив задачу: м'ютекс вільний, прапорець dirty не потрібен (запис уже в NVS).
    s_loc.set = true;
    weather_util::copyUtf8Trunc(s_loc.name, sizeof(s_loc.name), b.name);
    s_loc.lat = b.lat;
    s_loc.lon = b.lon;
    s_locSetFlag.store(true);
    publishNameLocked();
    WX_LOG("location loaded: \"%s\" (%.4f, %.4f)", s_loc.name, static_cast<double>(s_loc.lat),
           static_cast<double>(s_loc.lon));
}

// Записує поточне місце в NVS (або видаляє запис). Лише з задачі "weather".
void persistLocation() {
    WeatherService::Location snap;
    {
        Lock lk;
        if (!lk.ok()) {
            s_locDirty.store(true);  // повторимо на наступному колі
            return;
        }
        snap = s_loc;
    }
    Preferences prefs;
    if (!prefs.begin(cfg::kNvsNamespace, false)) {
        WX_LOG("NVS open failed, location not saved");
        return;
    }
    if (!snap.set) {
        prefs.remove(cfg::kNvsKey);
        WX_LOG("location cleared in NVS");
    } else {
        LocBlob b;
        memset(&b, 0, sizeof(b));
        b.version = cfg::kNvsBlobVersion;
        b.flags = 1;
        b.lat = snap.lat;
        b.lon = snap.lon;
        weather_util::copyUtf8Trunc(b.name, sizeof(b.name), snap.name);
        const size_t w = prefs.putBytes(cfg::kNvsKey, &b, sizeof(b));
        if (w != sizeof(b)) {
            WX_LOG("NVS write failed (%u)", static_cast<unsigned>(w));
        } else {
            WX_LOG("location saved: \"%s\"", snap.name);
        }
    }
    prefs.end();
}

// --- Умови запиту -------------------------------------------------------------------------------
// true — можна йти в мережу. Інакше err = no_wifi / no_time / low_memory.
bool preflight(char* err) {
    if (!WifiManager::isConnected()) {
        weather_util::copyUtf8Trunc(err, cfg::kErrorBytes, "no_wifi");
        return false;
    }
    if (!TimeService::isValid()) {
        weather_util::copyUtf8Trunc(err, cfg::kErrorBytes, "no_time");
        return false;
    }
    const size_t freeInt = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    const size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    if (freeInt < cfg::kMinFreeHeap || largest < cfg::kMinLargestBlock) {
        weather_util::copyUtf8Trunc(err, cfg::kErrorBytes, "low_memory");
        WX_LOG("postponed: free=%u largest=%u (need %u / %u)", static_cast<unsigned>(freeInt),
               static_cast<unsigned>(largest), static_cast<unsigned>(cfg::kMinFreeHeap),
               static_cast<unsigned>(cfg::kMinLargestBlock));
        return false;
    }
    return true;
}

// --- HTTPS ---------------------------------------------------------------------------------------
// Приймач тіла відповіді з обмеженням розміру (HTTPClient::writeToStream сам розбирає chunked).
class CapSink : public Stream {
public:
    CapSink(char* buf, size_t cap) : buf_(buf), cap_(cap) {}
    size_t write(uint8_t c) override { return write(&c, 1); }
    size_t write(const uint8_t* data, size_t n) override {
        if (len_ + n > cap_) {
            overflow_ = true;
            return 0;  // HTTPClient повертає HTTPC_ERROR_STREAM_WRITE
        }
        memcpy(buf_ + len_, data, n);
        len_ += n;
        return n;
    }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    size_t count() const { return len_; }
    bool overflow() const { return overflow_; }

private:
    char* buf_;
    size_t cap_;
    size_t len_ = 0;
    bool overflow_ = false;
};

void applyTrust(NetworkClientSecure& client) {
    if (cfg::kTlsInsecure) {
        client.setInsecure();  // УВАГА: без перевірки сертифіката (див. weather_config.h)
    } else {
        client.setCACertBundle(rootca_crt_bundle_start,
                               static_cast<size_t>(rootca_crt_bundle_end - rootca_crt_bundle_start));
    }
}

// Переводить помилку HTTPClient / код HTTP у короткий код для API.
void classifyError(int code, NetworkClientSecure& client, uint32_t elapsedMs, char* err) {
    if (code > 0) {
        snprintf(err, cfg::kErrorBytes, "http_%d", code);
    } else if (code == HTTPC_ERROR_READ_TIMEOUT) {
        weather_util::copyUtf8Trunc(err, cfg::kErrorBytes, "timeout");
    } else if (code == HTTPC_ERROR_TOO_LESS_RAM) {
        weather_util::copyUtf8Trunc(err, cfg::kErrorBytes, "low_memory");
    } else if (code == HTTPC_ERROR_STREAM_WRITE) {
        weather_util::copyUtf8Trunc(err, cfg::kErrorBytes, "parse");  // відповідь більша за буфер
    } else if (code == HTTPC_ERROR_CONNECTION_REFUSED) {
        char tlsMsg[80];
        // Код mbedTLS (великий від'ємний) = збій handshake / перевірки сертифіката -> "tls".
        // 0 або -1 («Generic error», з'єднання не встановлено): якщо чекали ≈ connect timeout -> "timeout"
        // (замовчане з'єднання, зокрема без інтернету), інакше швидка відмова (DNS / reset) -> "net".
        const int tlsErr = client.lastError(tlsMsg, sizeof(tlsMsg));
        if (tlsErr != 0 && tlsErr != -1) {
            WX_LOG("tls error %d: %s", tlsErr, tlsMsg);
            weather_util::copyUtf8Trunc(err, cfg::kErrorBytes, "tls");
        } else if (elapsedMs + 1000u >= cfg::kConnectTimeoutMs) {
            weather_util::copyUtf8Trunc(err, cfg::kErrorBytes, "timeout");
        } else {
            weather_util::copyUtf8Trunc(err, cfg::kErrorBytes, "net");
        }
    } else {
        weather_util::copyUtf8Trunc(err, cfg::kErrorBytes, "net");
    }
}

// GET https://… -> s_resp (NUL-термінований). true — HTTP 200 і тіло прийнято; інакше err.
bool fetchJson(const char* url, char* err, size_t& outLen) {
    outLen = 0;
    if (s_resp == nullptr) {
        weather_util::copyUtf8Trunc(err, cfg::kErrorBytes, "low_memory");
        return false;
    }

    const uint32_t t0 = millis();
    const size_t freeBefore = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t freeDuring = freeBefore;
    bool ok = false;
    {
        NetworkClientSecure client;
        applyTrust(client);
        client.setHandshakeTimeout(cfg::kHandshakeTimeoutS);

        HTTPClient http;
        http.setReuse(false);
        http.setUserAgent(cfg::kUserAgent);
        http.setConnectTimeout(static_cast<int32_t>(cfg::kConnectTimeoutMs));
        http.setTimeout(static_cast<uint16_t>(cfg::kHttpTimeoutMs));

        if (!http.begin(client, url)) {
            weather_util::copyUtf8Trunc(err, cfg::kErrorBytes, "net");
        } else {
            const int code = http.GET();
            // Сесія TLS і буфери mbedTLS живі саме зараз: це найнижча вільна RAM запиту.
            freeDuring = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
            if (code != HTTP_CODE_OK) {
                classifyError(code, client, millis() - t0, err);
            } else {
                CapSink sink(s_resp, cfg::kMaxResponseBytes - 1);
                const int ret = http.writeToStream(&sink);
                if (ret < 0 || sink.overflow()) {
                    classifyError(sink.overflow() ? HTTPC_ERROR_STREAM_WRITE : ret, client, millis() - t0, err);
                } else if (sink.count() == 0) {
                    weather_util::copyUtf8Trunc(err, cfg::kErrorBytes, "parse");
                } else {
                    outLen = sink.count();
                    s_resp[outLen] = '\0';
                    ok = true;
                }
            }
            http.end();
        }
    }  // client / http знищено: пам'ять TLS звільнено

    WX_LOG("tls heap: before=%u during=%u after=%u minEver=%u stackHW=%u ms=%u %s%s",
           static_cast<unsigned>(freeBefore), static_cast<unsigned>(freeDuring),
           static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
           static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)),
           static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)),
           static_cast<unsigned>(millis() - t0), ok ? "ok" : "err=", ok ? "" : err);
    return ok;
}

// --- Пошук міста ---------------------------------------------------------------------------------
void finishSearch(WeatherService::SearchPhase phase, const char* err,
                  const WeatherService::SearchState* results) {
    // Не лишаємо фазу Searching назавжди: тут чекаємо м'ютекс довше, ніж у геттерах.
    if (s_mutex == nullptr || xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
    if (results != nullptr) {
        s_search.count = results->count;
        memcpy(s_search.results, results->results, sizeof(s_search.results));
    } else {
        s_search.count = 0;
    }
    s_search.phase = phase;
    weather_util::copyUtf8Trunc(s_search.error, sizeof(s_search.error), err != nullptr ? err : "");
    xSemaphoreGive(s_mutex);
}

void doSearch(const char* query) {
    char err[cfg::kErrorBytes] = {0};
    if (!preflight(err)) {
        WX_LOG("search blocked: %s", err);
        finishSearch(WeatherService::SearchPhase::Error, err, nullptr);
        return;
    }

    size_t qlen = strlen(query);
    char enc[cfg::kQueryMaxBytes * 3 + 1];
    if (weather_util::urlEncode(query, qlen, enc, sizeof(enc)) == 0) {
        finishSearch(WeatherService::SearchPhase::Error, "parse", nullptr);
        return;
    }
    char url[cfg::kUrlBytes];
    const int n = snprintf(url, sizeof(url), cfg::kGeocodeUrlFmt, enc, static_cast<unsigned>(cfg::kMaxCities));
    if (n <= 0 || static_cast<size_t>(n) >= sizeof(url)) {
        finishSearch(WeatherService::SearchPhase::Error, "parse", nullptr);
        return;
    }
    WX_LOG("search \"%s\"", query);

    size_t len = 0;
    if (!fetchJson(url, err, len)) {
        finishSearch(WeatherService::SearchPhase::Error, err, nullptr);
        return;
    }

    JsonDocument filter;
    JsonObject f = filter["results"][0].to<JsonObject>();
    f["name"] = true;
    f["admin1"] = true;
    f["country"] = true;
    f["latitude"] = true;
    f["longitude"] = true;

    JsonDocument doc;
    const DeserializationError de = deserializeJson(doc, s_resp, len, DeserializationOption::Filter(filter));
    if (de) {
        WX_LOG("search parse error: %s", de.c_str());
        finishSearch(WeatherService::SearchPhase::Error, "parse", nullptr);
        return;
    }

    WeatherService::SearchState tmp;
    memset(&tmp, 0, sizeof(tmp));
    JsonArrayConst arr = doc["results"].as<JsonArrayConst>();  // немає results -> порожній масив
    for (JsonObjectConst r : arr) {
        if (tmp.count >= cfg::kMaxCities) break;
        const char* name = r["name"] | "";
        if (name[0] == '\0') continue;
        if (!r["latitude"].is<float>() || !r["longitude"].is<float>()) continue;
        const float lat = r["latitude"].as<float>();
        const float lon = r["longitude"].as<float>();
        if (!validLat(lat) || !validLon(lon)) continue;

        WeatherService::Candidate& c = tmp.results[tmp.count];
        weather_util::copyUtf8Trunc(c.name, sizeof(c.name), name);
        weather_util::copyUtf8Trunc(c.admin1, sizeof(c.admin1), r["admin1"] | "");
        weather_util::copyUtf8Trunc(c.country, sizeof(c.country), r["country"] | "");
        c.lat = lat;
        c.lon = lon;
        ++tmp.count;
    }
    WX_LOG("search done: %u result(s)", static_cast<unsigned>(tmp.count));
    finishSearch(tmp.count > 0 ? WeatherService::SearchPhase::Done : WeatherService::SearchPhase::NotFound,
                 nullptr, &tmp);
}

// --- Оновлення погоди -----------------------------------------------------------------------------
enum class RefreshResult : uint8_t { Ok, Blocked, LowMemory, Failed, Discarded };

RefreshResult doRefresh() {
    WeatherService::Location loc;
    uint32_t gen;
    {
        Lock lk;
        if (!lk.ok()) return RefreshResult::Blocked;
        loc = s_loc;
        gen = s_locGen;
    }
    if (!loc.set) return RefreshResult::Discarded;

    char err[cfg::kErrorBytes] = {0};
    if (!preflight(err)) {
        setError(err);
        return strcmp(err, "low_memory") == 0 ? RefreshResult::LowMemory : RefreshResult::Blocked;
    }

    char url[cfg::kUrlBytes];
    const int n = snprintf(url, sizeof(url), cfg::kForecastUrlFmt, static_cast<double>(loc.lat),
                           static_cast<double>(loc.lon));
    if (n <= 0 || static_cast<size_t>(n) >= sizeof(url)) {
        setError("parse");
        return RefreshResult::Failed;
    }

    size_t len = 0;
    if (!fetchJson(url, err, len)) {
        setError(err);
        WX_LOG("refresh failed: %s", err);
        return RefreshResult::Failed;
    }

    JsonDocument filter;
    JsonObject fc = filter["current"].to<JsonObject>();
    fc["temperature_2m"] = true;
    fc["weather_code"] = true;
    fc["is_day"] = true;
    fc["wind_speed_10m"] = true;
    fc["relative_humidity_2m"] = true;

    JsonDocument doc;
    const DeserializationError de = deserializeJson(doc, s_resp, len, DeserializationOption::Filter(filter));
    if (de) {
        WX_LOG("refresh parse error: %s", de.c_str());
        setError("parse");
        return RefreshResult::Failed;
    }
    JsonObjectConst c = doc["current"].as<JsonObjectConst>();
    if (c.isNull() || !c["temperature_2m"].is<float>() || !c["weather_code"].is<int>() ||
        !c["is_day"].is<int>() || !c["wind_speed_10m"].is<float>() ||
        !c["relative_humidity_2m"].is<float>()) {
        WX_LOG("refresh parse error: missing fields");
        setError("parse");
        return RefreshResult::Failed;
    }
    const float temp = c["temperature_2m"].as<float>();
    const int code = c["weather_code"].as<int>();
    const int isDay = c["is_day"].as<int>();
    const float wind = c["wind_speed_10m"].as<float>();
    const float hum = c["relative_humidity_2m"].as<float>();
    if (!weather_util::inRange(temp, cfg::kTempMinC, cfg::kTempMaxC) || code < 0 || code > 99 ||
        (isDay != 0 && isDay != 1) || !weather_util::inRange(wind, 0.0f, cfg::kWindMaxMs) ||
        !weather_util::inRange(hum, 0.0f, 100.0f)) {
        WX_LOG("refresh parse error: value out of range");
        setError("parse");
        return RefreshResult::Failed;
    }

    WeatherService::Current cur;
    cur.tempC = weather_util::round1(temp);
    cur.code = static_cast<int16_t>(code);
    cur.isDay = (isDay == 1);
    cur.windMs = weather_util::round1(wind);
    cur.humidity = static_cast<uint8_t>(lroundf(hum));
    cur.fetchedAtMs = millis();

    {
        Lock lk;
        if (!lk.ok()) return RefreshResult::Blocked;
        if (!s_loc.set || s_locGen != gen) {
            WX_LOG("refresh result discarded: location changed");
            return RefreshResult::Discarded;
        }
        s_cur = cur;
        s_hasData = true;
        s_hasDataFlag.store(true);
        s_error[0] = '\0';
    }
    WX_LOG("refresh ok: %.1f C code=%d day=%d wind=%.1f m/s hum=%u%%", static_cast<double>(cur.tempC),
           code, isDay, static_cast<double>(cur.windMs), static_cast<unsigned>(cur.humidity));
    return RefreshResult::Ok;
}

// --- Задача ----------------------------------------------------------------------------------------
void taskMain(void* /*arg*/) {
    uint32_t nextRefreshAt = millis();
    bool refreshPending = s_locSetFlag.load();
    uint8_t failCount = 0;
#if WEATHER_TEST
    uint32_t lastTestPrint = millis();
#endif

    for (;;) {
        Cmd cmd;
        if (xQueueReceive(s_queue, &cmd, pdMS_TO_TICKS(cfg::kPollMs)) == pdTRUE) {
            doSearch(cmd.query);
        }

        if (s_locDirty.exchange(false)) {
            persistLocation();
            failCount = 0;
            nextRefreshAt = millis();
            refreshPending = s_locSetFlag.load();
        }

        if (refreshPending && static_cast<int32_t>(millis() - nextRefreshAt) >= 0) {
            switch (doRefresh()) {
                case RefreshResult::Ok:
                    failCount = 0;
                    nextRefreshAt = millis() + cfg::kRefreshMs;
                    break;
                case RefreshResult::Blocked:
                    nextRefreshAt = millis() + cfg::kBlockedRetryMs;
                    break;
                case RefreshResult::LowMemory:
                    nextRefreshAt = millis() + cfg::kLowMemRetryMs;
                    break;
                case RefreshResult::Failed: {
                    const uint8_t step = failCount < cfg::kRetryStepCount ? failCount : cfg::kRetryStepCount - 1;
                    nextRefreshAt = millis() + cfg::kRetryMs[step];
                    if (failCount < 255) ++failCount;
                    WX_LOG("retry in %u s", static_cast<unsigned>(cfg::kRetryMs[step] / 1000u));
                    break;
                }
                case RefreshResult::Discarded:
                    // Місце змінили / очистили під час запиту: прапорець s_locDirty уже піднято.
                    nextRefreshAt = millis();
                    refreshPending = s_locSetFlag.load();
                    break;
            }
        }

#if WEATHER_TEST
        if (millis() - lastTestPrint >= cfg::kTestPeriodMs) {
            lastTestPrint = millis();
            WeatherService::Status st;
            const bool got = WeatherService::getStatus(st);
            Serial.printf("[WX] test free=%u largest=%u minEver=%u stackHW=%u loc=%d valid=%d age=%u err=%s\n",
                          static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                          static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)),
                          static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)),
                          static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)),
                          got && st.loc.set ? 1 : 0, got && st.valid ? 1 : 0,
                          got ? static_cast<unsigned>(st.ageMs) : 0u, got ? st.error : "?");
        }
#endif
    }
}

}  // namespace

// ==================================================================================================
// Публічний API
// ==================================================================================================

bool WeatherService::begin() {
    if (s_started) return true;

    s_mutex = xSemaphoreCreateMutex();
    s_queue = xQueueCreate(cfg::kQueueLen, sizeof(Cmd));
    if (s_mutex == nullptr || s_queue == nullptr) {
        WX_LOG("begin failed: mutex/queue");
        return false;
    }

    // Буфер відповіді — у PSRAM (правило 8); без PSRAM — запасний варіант у внутрішній RAM.
    s_resp = static_cast<char*>(heap_caps_malloc(cfg::kMaxResponseBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (s_resp == nullptr) {
        s_resp = static_cast<char*>(heap_caps_malloc(cfg::kMaxResponseBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        WX_LOG("response buffer in internal RAM (no PSRAM)");
    }
    if (s_resp == nullptr) {
        WX_LOG("begin failed: response buffer");
        return false;
    }

    memset(&s_search, 0, sizeof(s_search));
    s_search.phase = SearchPhase::Idle;
    publishNameLocked();
    loadLocationFromNvs();

    const BaseType_t ok = xTaskCreatePinnedToCore(taskMain, "weather", cfg::kTaskStackBytes, nullptr,
                                                  cfg::kTaskPriority, &s_task, cfg::kTaskCore);
    if (ok != pdPASS) {
        s_task = nullptr;
        WX_LOG("begin failed: task create");
        return false;
    }
    s_started = true;
    WX_LOG("started (core %d, prio %u, stack %u)", cfg::kTaskCore, static_cast<unsigned>(cfg::kTaskPriority),
           static_cast<unsigned>(cfg::kTaskStackBytes));
    return true;
}

bool WeatherService::hasLocation() { return s_locSetFlag.load(); }

bool WeatherService::hasData() { return s_hasDataFlag.load(); }

bool WeatherService::getCurrent(Current& out) {
    Lock lk;
    if (!lk.ok() || !s_hasData) return false;
    out = s_cur;
    return true;
}

uint32_t WeatherService::ageMs() {
    Lock lk;
    if (!lk.ok() || !s_hasData) return kNoAge;
    return millis() - s_cur.fetchedAtMs;
}

const char* WeatherService::cityName() { return s_nameBuf[s_nameIdx.load()]; }

bool WeatherService::getLocation(Location& out) {
    Lock lk;
    if (!lk.ok()) return false;
    out = s_loc;
    return true;
}

bool WeatherService::getStatus(Status& out) {
    Lock lk;
    if (!lk.ok()) return false;
    out.loc = s_loc;
    out.valid = s_hasData && s_loc.set;
    out.cur = s_cur;
    out.ageMs = out.valid ? (millis() - s_cur.fetchedAtMs) : kNoAge;
    memcpy(out.error, s_error, sizeof(out.error));
    return true;
}

WeatherService::SearchRequest WeatherService::requestSearch(const char* query) {
    if (!s_started) return SearchRequest::Unavailable;
    size_t len = 0;
    const char* q = weather_util::trimSpan(query, len);
    if (len == 0) return SearchRequest::Empty;
    if (len > cfg::kQueryMaxBytes) return SearchRequest::TooLong;

    Cmd cmd;
    memcpy(cmd.query, q, len);
    cmd.query[len] = '\0';
    if (!weather_util::isTextOk(cmd.query, cfg::kQueryMaxBytes)) return SearchRequest::BadChars;

    {
        Lock lk;
        if (!lk.ok()) return SearchRequest::Unavailable;
        if (s_search.phase == SearchPhase::Searching) return SearchRequest::Busy;
        s_search.phase = SearchPhase::Searching;
        s_search.count = 0;
        s_search.error[0] = '\0';
        memcpy(s_search.query, cmd.query, len + 1);
    }
    if (xQueueSend(s_queue, &cmd, 0) != pdTRUE) {
        Lock lk;
        if (lk.ok()) s_search.phase = SearchPhase::Idle;
        return SearchRequest::Unavailable;
    }
    return SearchRequest::Accepted;
}

bool WeatherService::getSearchState(SearchState& out) {
    Lock lk;
    if (!lk.ok()) return false;
    out = s_search;
    return true;
}

bool WeatherService::selectCandidate(int index) {
    if (!s_started) return false;
    Candidate c;
    {
        Lock lk;
        if (!lk.ok()) return false;
        if (s_search.phase != SearchPhase::Done || index < 0 || index >= static_cast<int>(s_search.count)) {
            return false;
        }
        c = s_search.results[index];
    }
    const bool ok = applyLocation(c.name, c.lat, c.lon);
    if (ok) WX_LOG("location selected: \"%s\" (%.4f, %.4f)", c.name, static_cast<double>(c.lat),
                   static_cast<double>(c.lon));
    return ok;
}

bool WeatherService::setLocation(const char* name, float lat, float lon) {
    if (!s_started) return false;
    if (!weather_util::isTextOk(name, cfg::kNameBytes - 1) || !validLat(lat) || !validLon(lon)) return false;
    const bool ok = applyLocation(name, lat, lon);
    if (ok) WX_LOG("location set: \"%s\" (%.4f, %.4f)", name, static_cast<double>(lat), static_cast<double>(lon));
    return ok;
}

void WeatherService::clearLocation() {
    if (!s_started) return;
    Lock lk;
    if (!lk.ok()) return;
    s_loc.set = false;
    s_loc.name[0] = '\0';
    s_loc.lat = 0.0f;
    s_loc.lon = 0.0f;
    ++s_locGen;
    s_hasData = false;
    s_hasDataFlag.store(false);
    s_error[0] = '\0';
    s_locSetFlag.store(false);
    publishNameLocked();
    s_locDirty.store(true);
    WX_LOG("location cleared");
}