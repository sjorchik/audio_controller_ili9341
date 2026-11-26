#pragma once

// [S2a] Сервіс часу: SNTP (ESP-IDF esp_sntp) + локальний час Києва (POSIX TZ).
//
// Старт: TimeService::begin() викликається один раз у setup() після WifiManager::begin().
// SNTP запускається ОДИН раз, коли Wi-Fi (STA) отримав IP (подія ARDUINO_EVENT_WIFI_STA_GOT_IP;
// якщо до begin() Wi-Fi уже підключений — одразу). У режимі AP / без Wi-Fi SNTP не стартує.
// Повторні синхронізації веде сам SNTP-сервіс IDF.
//
// Усі методи неблокуючі й безпечні з будь-якого ядра / задачі. Власних мережевих задач немає
// (під TIME_TEST — одна мала діагностична задача). У NVS нічого не пишеться.
//
// Час ДІЙСНИЙ лише після першої успішної SNTP-синхронізації в цьому запуску. Після перезапуску
// (навіть якщо системний час у чіпі зберігся) isValid() = false до нової синхронізації.

#include <stdint.h>
#include <time.h>

#include "config/time_config.h"

class TimeService {
public:
    // Встановлює часовий пояс, підписується на подію Wi-Fi. Повторний виклик безпечний.
    // false — не вдалося задати TZ.
    static bool begin();

    // Час синхронізовано хоча б раз у цьому запуску І epoch > time_cfg::kValidEpochMin.
    static bool isValid();

    // Локальний час (Київ). false (out не змінюється), якщо час недійсний.
    static bool getLocal(tm& out);

    // Те саме + epoch (UTC) з ТОГО Ж зчитування часу (без розбіжності між полями).
    static bool getNow(time_t& epochOut, tm& localOut);

    // Мілісекунди від останньої успішної синхронізації; time_cfg::kNeverSyncedMs, якщо ще не було.
    // Лічильник 32-бітний (перехід через 0 приблизно раз на 49,7 доби).
    static uint32_t msSinceSync();

    // time_cfg::kTzName ("EET/EEST").
    static const char* tzName();
};
