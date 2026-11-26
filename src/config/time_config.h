#pragma once

// [S2a] Налаштування сервісу часу (core/time_service.*) і GET /api/time.

#include <stdint.h>

// Serial-лог сервісу: "[TIME] waiting for Wi-Fi", "[TIME] sync started", "[TIME] synced ...".
// Вимкнути = 0.
#define TIME_DEBUG 1

// Тестовий режим для hwcheck: раз на секунду друкує "[TIME] local=... valid=... sinceSync=...",
// а при старті — одноразову перевірку переходу на літній / зимовий час на фіксованих епохах
// (без чекання). Створює одну малу задачу. У релізі = 0.
#define TIME_TEST 0

namespace time_cfg {

// Часовий пояс (Київ) у форматі POSIX TZ: EET = UTC+2, EEST = UTC+3;
// літній час — від останньої неділі березня 03:00 до останньої неділі жовтня 04:00 (= 01:00 UTC).
constexpr const char* kTzPosix = "EET-2EEST,M3.5.0/3,M10.5.0/4";

// Назва поясу для API (`tz`) і TimeService::tzName().
constexpr const char* kTzName = "EET/EEST";

// SNTP-сервери. Другий використовується, лише якщо lwIP зібрано з SNTP_MAX_SERVERS > 1.
constexpr const char* kNtpServer1 = "pool.ntp.org";
constexpr const char* kNtpServer2 = "time.google.com";

// Нижня межа «розумного» epoch (2023-11-14): все, що менше, вважається недійсним часом.
constexpr int64_t kValidEpochMin = 1700000000;

// Значення TimeService::msSinceSync() до першої синхронізації.
constexpr uint32_t kNeverSyncedMs = 0xFFFFFFFFu;

// Період повторної синхронізації НЕ задається тут: його веде SNTP-сервіс IDF
// (CONFIG_LWIP_SNTP_UPDATE_DELAY, за замовчуванням 1 година); дублювати не потрібно.

// --- Лише для TIME_TEST ---
constexpr uint32_t kTestPeriodMs = 1000;
constexpr uint32_t kTestTaskStackBytes = 4096;
constexpr uint8_t kTestTaskPriority = 1;  // найнижчий, як у решти діагностичних задач
constexpr int kTestTaskCore = 0;

// Епохи довкола переходів 2027 року (UTC): за Києвом
//   1806195599 -> 2027-03-28 02:59:59 EET  (isdst 0)
//   1806195600 -> 2027-03-28 04:00:00 EEST (isdst 1)
//   1824944399 -> 2027-10-31 03:59:59 EEST (isdst 1)
//   1824944400 -> 2027-10-31 03:00:00 EET  (isdst 0)
constexpr int64_t kTestDstEpochs[4] = {1806195599, 1806195600, 1824944399, 1824944400};

}  // namespace time_cfg
