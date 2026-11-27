#pragma once

// [S2b] Налаштування сервісу погоди (core/weather_service.*), кодів WMO (core/weather_codes.h)
// і маршрутів /api/weather* (net/web_api_weather.*).
// [S2b2] Транспорт за замовчуванням — звичайний HTTP (без TLS); окремі пороги пам'яті для HTTP;
// HTTPS лишено як опцію (kUseHttps) і як одноразовий запасний варіант після перенаправлення.

#include <stddef.h>
#include <stdint.h>

// Serial-лог сервісу з тегом [WX]. Вимкнути = 0.
#define WEATHER_DEBUG 1

// Тестовий режим для hwcheck: раз на kTestPeriodMs друкує стан сервісу, вільну внутрішню RAM
// (free / largest / minEver) і запас стека задачі; [S2b2] на КОЖЕН мережевий запит друкує транспорт
// (http / https), час запиту, вільну RAM до / мінімум за запит / після, largest block, stackHW
// і запас до порога. У релізі = 0.
#define WEATHER_TEST 0

namespace weather_cfg {

// --- Періоди ---------------------------------------------------------------------------------
// Період оновлення після УСПІШНОГО запиту.
constexpr uint32_t kRefreshMs = 15u * 60u * 1000u;

// Повтори після помилки мережі (http_<код> / redirect / tls / timeout / net / parse): 30 с -> 60 с -> 5 хв,
// далі завжди останнє значення (5 хв). Скидається першим успіхом або зміною міста.
constexpr uint32_t kRetryMs[] = {30u * 1000u, 60u * 1000u, 5u * 60u * 1000u};
constexpr uint8_t kRetryStepCount = sizeof(kRetryMs) / sizeof(kRetryMs[0]);

// Період пробудження задачі без команд (також затримка реакції на зміну міста).
constexpr uint32_t kPollMs = 500;

// Повторна перевірка, коли запит заблоковано умовами no_wifi / no_time (мережу НЕ чіпаємо).
constexpr uint32_t kBlockedRetryMs = 2000;

// Повторна перевірка після low_memory.
constexpr uint32_t kLowMemRetryMs = 30u * 1000u;

// --- Транспорт [S2b2] ---------------------------------------------------------------------------
// false (за замовчуванням, РІШЕННЯ ВЛАСНИКА 2026-11-27): запити йдуть по звичайному HTTP (порт 80,
// клієнт без TLS; TLS-клас і сертифікати не створюються). Дані публічні, ключів немає.
// Перевірено curl.exe з ПК: http://api.open-meteo.com і http://geocoding-api.open-meteo.com дають 200
// без перенаправлення; прогноз приходить з Transfer-Encoding: chunked (HTTPClient::writeToStream
// розбирає chunked сам).
// true: усі запити по HTTPS (як у S2b; TLS у піку ≈ 44–46 КБ внутрішньої RAM, пороги kMinFreeHeap /
// kMinLargestBlock).
constexpr bool kUseHttps = false;

// Якщо HTTP-відповідь — перенаправлення (301 / 302 / 307 / 308): автоматично НЕ слідуємо (HTTPClient
// follow-redirects вимкнено), код помилки = "redirect". При true й kUseHttps = false пристрій ОДИН раз
// (на цей запит) пробує той самий запит по HTTPS, але лише якщо вільної RAM вистачає за порогами
// HTTPS (kMinFreeHeap / kMinLargestBlock); інакше лишається "redirect" і повтор за звичайною
// затримкою. Стійкого «запам'ятовування» не робить: наступний запит знову починається з HTTP.
constexpr bool kHttpsFallback = true;

// --- Пам'ять (перед кожним запитом) -----------------------------------------------------------
// Мінімум вільної ВНУТРІШНЬОЇ RAM і найбільшого вільного блоку. Нижче — запит відкладається
// (код помилки low_memory, повтор через kLowMemRetryMs).
//
// HTTPS (TLS) — виміряно на залізі (hwcheck S2b):
//   Standby: вільно ≈ 73–77 КБ (largest ≈ 32 КБ)  — запит проходить;
//   лінійний вхід: ≈ 53 КБ (largest ≈ 31 КБ)       — TLS (≈ 45 КБ у піку) не поміститься, блокується;
//   радіо: ≈ 27 КБ (largest ≈ 14 КБ)               — блокується.
// 68 КБ: Standby проходить (мінімум, що спостерігався, 72,9 КБ), решта режимів — ні.
constexpr size_t kMinFreeHeap = 68u * 1024u;
constexpr size_t kMinLargestBlock = 24u * 1024u;

// HTTP (без TLS) — ПОЧАТКОВІ значення, ОЦІНКА (уточнити за логом `[WX] net http ...` у hwcheck S2b2):
//   пік запиту ≈ 8–14 КБ внутрішньої RAM (TCP-сокет lwIP ≈ 2 КБ, приймальні pbuf до одного вікна TCP
//   ≈ 3–6 КБ, рядки HTTPClient ≈ 0,5 КБ, JsonDocument з фільтром ≈ 1–2 КБ; задача weather уже існує, її
//   стек не рахується); найбільший одиночний блок ≈ 1,5–3 КБ.
//   Запас для аудіо / Wi-Fi (динамічні приймальні буфери Wi-Fi беруть внутрішню RAM у потоці) ≈ 8 КБ.
//   Поріг ≈ пік (≈ 14) + запас (≈ 8) = 22 КБ; на радіо вільно ≈ 26–27 КБ, тож запит проходить, а після
//   нього лишається ≳ 12 КБ. Найбільший блок — 8 КБ (радіо має ≈ 14 КБ; менше = сильна фрагментація).
//   Якщо виміряний `used` (до мінімуму за запит) більший, поріг треба піднімати: kMinFreeHeapHttp
//   ≥ used + 8 КБ.
constexpr size_t kMinFreeHeapHttp = 20u * 1024u;
constexpr size_t kMinLargestBlockHttp = 8u * 1024u;

static_assert(kMinFreeHeapHttp <= kMinFreeHeap, "HTTP threshold must not exceed HTTPS threshold");
static_assert(kMinLargestBlockHttp <= kMinLargestBlock, "HTTP threshold must not exceed HTTPS threshold");

// --- Мережа / TLS -----------------------------------------------------------------------------
// Лише для HTTPS-гілки (kUseHttps = true або запасний варіант після перенаправлення).
// false: перевірка сертифікатів за вбудованим бандлом (потрібен коректний час).
// true: БЕЗ перевірки сертифіката (setInsecure()) — ЛИШЕ запасний варіант, якщо бандл недоступний
// у збірці (помилка лінкування _binary_x509_crt_bundle_start). Трафік можна підмінити.
constexpr bool kTlsInsecure = false;

constexpr uint32_t kConnectTimeoutMs = 8000;   // TCP-зʼєднання
constexpr uint32_t kHandshakeTimeoutS = 10;    // TLS handshake (у бібліотеки за замовчуванням 120 с)
constexpr uint32_t kHttpTimeoutMs = 8000;      // читання відповіді
constexpr size_t kMaxResponseBytes = 6144;     // буфер відповіді (PSRAM); відповідь більша = parse
constexpr size_t kUrlBytes = 384;              // хост + шлях (без схеми), байт
constexpr const char* kUserAgent = "AudioController-Weather/1.0";

// Open-Meteo (без ключа). Рядки БЕЗ схеми: "http://" або "https://" додає сервіс залежно від
// kUseHttps / запасного варіанту [S2b2]. Формат: широта, довгота (%.4f).
constexpr const char* kForecastUrlFmt =
    "api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
    "&current=temperature_2m,weather_code,is_day,wind_speed_10m,relative_humidity_2m"
    "&wind_speed_unit=ms&timezone=auto";

// Формат: URL-кодований запит, кількість кандидатів (теж без схеми).
constexpr const char* kGeocodeUrlFmt =
    "geocoding-api.open-meteo.com/v1/search?name=%s&count=%u&language=uk&format=json";

// --- Задача ----------------------------------------------------------------------------------
// Ядро 0 (аудіо на ядрі 1), найнижчий пріоритет мережі. Стек 10 КБ: TLS handshake (RSA / ECDSA) і
// HTTPClient беруть стек викликача; розмір перевіряється за логом `stackHW` (WEATHER_TEST / DEBUG).
// HTTP-гілка стека потребує менше; значення НЕ змінюємо (потрібне для HTTPS-гілки).
constexpr int kTaskCore = 0;
constexpr uint8_t kTaskPriority = 1;
constexpr uint32_t kTaskStackBytes = 10240;
constexpr uint8_t kQueueLen = 2;

// Максимальне очікування м'ютекса в геттерах (геттери не блокують надовго).
constexpr uint32_t kLockWaitMs = 10;

// --- Дані й межі -----------------------------------------------------------------------------
constexpr size_t kNameBytes = 32;       // назва міста: до 31 байта UTF-8 + NUL
constexpr size_t kRegionBytes = 48;     // admin1 / country кандидата (відображення), + NUL
constexpr size_t kQueryMaxBytes = 64;   // запит пошуку, байт UTF-8 (після обрізання пробілів)
constexpr uint8_t kMaxCities = 5;       // кандидатів у відповіді geocoding
constexpr size_t kErrorBytes = 16;      // код помилки ("http_503", "redirect" тощо) + NUL

// Допустимі межі значень у відповіді Open-Meteo (поза ними — помилка parse).
constexpr float kTempMinC = -90.0f;
constexpr float kTempMaxC = 70.0f;
constexpr float kWindMaxMs = 150.0f;

// --- NVS (окремо від Settings; kFormatVersion = 11 не чіпається) ---------------------------------
constexpr const char* kNvsNamespace = "wx";
constexpr const char* kNvsKey = "loc";
constexpr uint8_t kNvsBlobVersion = 1;

// --- Лише для WEATHER_TEST ---------------------------------------------------------------------
constexpr uint32_t kTestPeriodMs = 5000;

}  // namespace weather_cfg
