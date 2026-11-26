// core/time_service.cpp [S2a]: сервіс часу (SNTP + часовий пояс).

#include "core/time_service.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <stdio.h>
#include <stdlib.h>

#include <atomic>

#include "config/time_config.h"
#include "net/wifi_manager.h"

#if TIME_DEBUG
#define TIME_LOG(...)                   \
    do {                                \
        Serial.print("[TIME] ");        \
        Serial.printf(__VA_ARGS__);     \
    } while (0)
#else
#define TIME_LOG(...) \
    do {              \
    } while (0)
#endif

namespace {

std::atomic<bool> s_beginDone{false};
std::atomic<bool> s_sntpStarted{false};
std::atomic<bool> s_synced{false};          // була хоча б одна успішна синхронізація
std::atomic<uint32_t> s_lastSyncMs{0};      // millis() останньої синхронізації

// Форматує локальний час як "YYYY-MM-DD HH:MM:SS" (для логу).
void formatLocal(time_t epoch, char* out, size_t cap) {
    tm t;
    localtime_r(&epoch, &t);
    snprintf(out, cap, "%04d-%02d-%02d %02d:%02d:%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
             t.tm_hour, t.tm_min, t.tm_sec);
}

// Колбек SNTP (виконується в задачі tcpip_thread lwIP): коротко, без блокувань.
// Час уже виставлено системою (режим SNTP_SYNC_MODE_IMMED за замовчуванням).
void onSynced(struct timeval*) {
    const time_t now = time(nullptr);
    if (now <= static_cast<time_t>(time_cfg::kValidEpochMin)) {
        TIME_LOG("not valid (epoch=%lld)\n", static_cast<long long>(now));
        return;
    }
    s_lastSyncMs.store(millis(), std::memory_order_relaxed);
    s_synced.store(true, std::memory_order_release);
#if TIME_DEBUG
    char buf[24];
    formatLocal(now, buf, sizeof(buf));
    TIME_LOG("synced %s\n", buf);
#endif
}

// Ідемпотентний запуск SNTP (викликається з події Wi-Fi й з begin()).
void startSntp() {
    bool expected = false;
    if (!s_sntpStarted.compare_exchange_strong(expected, true)) {
        return;  // уже запущено; повторні GOT_IP (перепідключення) нічого не міняють
    }
    TIME_LOG("sync started\n");

    if (esp_sntp_enabled()) {
        esp_sntp_stop();  // хтось інший встиг запустити SNTP: беремо керування на себе
    }
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, time_cfg::kNtpServer1);
#if defined(SNTP_MAX_SERVERS) && (SNTP_MAX_SERVERS > 1)
    esp_sntp_setservername(1, time_cfg::kNtpServer2);
#endif
    sntp_set_time_sync_notification_cb(onSynced);
    esp_sntp_init();
}

// Подія Arduino: STA отримав IP.
void onWifiEvent(arduino_event_id_t) {
    startSntp();
}

#if TIME_TEST
void testTask(void*) {
    // Одноразова перевірка переходу на літній / зимовий час на фіксованих епохах (без чекання).
    for (int i = 0; i < 4; ++i) {
        const time_t e = static_cast<time_t>(time_cfg::kTestDstEpochs[i]);
        tm t;
        localtime_r(&e, &t);
        Serial.printf("[TIME] dst-check epoch=%lld local=%04d-%02d-%02d %02d:%02d:%02d isdst=%d\n",
                      static_cast<long long>(e), t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                      t.tm_hour, t.tm_min, t.tm_sec, t.tm_isdst);
    }
    for (;;) {
        tm t;
        const bool ok = TimeService::getLocal(t);
        const uint32_t since = TimeService::msSinceSync();
        char loc[24] = "-";
        if (ok) {
            snprintf(loc, sizeof(loc), "%04d-%02d-%02d %02d:%02d:%02d", t.tm_year + 1900,
                     t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
        }
        if (since == time_cfg::kNeverSyncedMs) {
            Serial.printf("[TIME] local=%s valid=%d sinceSync=never\n", loc, ok ? 1 : 0);
        } else {
            Serial.printf("[TIME] local=%s valid=%d sinceSync=%lu\n", loc, ok ? 1 : 0,
                          static_cast<unsigned long>(since));
        }
        vTaskDelay(pdMS_TO_TICKS(time_cfg::kTestPeriodMs));
    }
}
#endif

}  // namespace

bool TimeService::begin() {
    bool expected = false;
    if (!s_beginDone.compare_exchange_strong(expected, true)) {
        return true;
    }

    // TZ задаємо один раз, до будь-яких читань часу з інших задач.
    if (setenv("TZ", time_cfg::kTzPosix, 1) != 0) {
        Serial.println("[TIME] ERROR: setenv(TZ) failed");
        s_beginDone.store(false);
        return false;
    }
    tzset();

    // Спершу підписка, потім перевірка поточного стану: так подію GOT_IP не пропустимо.
    WiFi.onEvent(onWifiEvent, ARDUINO_EVENT_WIFI_STA_GOT_IP);
    if (WifiManager::isConnected()) {
        startSntp();
    } else {
        TIME_LOG("waiting for Wi-Fi\n");
    }

#if TIME_TEST
    xTaskCreatePinnedToCore(testTask, "time_test", time_cfg::kTestTaskStackBytes, nullptr,
                            time_cfg::kTestTaskPriority, nullptr, time_cfg::kTestTaskCore);
#endif
    return true;
}

bool TimeService::isValid() {
    if (!s_synced.load(std::memory_order_acquire)) return false;
    return time(nullptr) > static_cast<time_t>(time_cfg::kValidEpochMin);
}

bool TimeService::getNow(time_t& epochOut, tm& localOut) {
    if (!s_synced.load(std::memory_order_acquire)) return false;
    const time_t now = time(nullptr);
    if (now <= static_cast<time_t>(time_cfg::kValidEpochMin)) return false;
    tm t;
    localtime_r(&now, &t);  // потокобезпечна (власні дані в t; TZ не змінюється після begin())
    epochOut = now;
    localOut = t;
    return true;
}

bool TimeService::getLocal(tm& out) {
    time_t e;
    return getNow(e, out);
}

uint32_t TimeService::msSinceSync() {
    if (!s_synced.load(std::memory_order_acquire)) return time_cfg::kNeverSyncedMs;
    return millis() - s_lastSyncMs.load(std::memory_order_relaxed);
}

const char* TimeService::tzName() {
    return time_cfg::kTzName;
}
