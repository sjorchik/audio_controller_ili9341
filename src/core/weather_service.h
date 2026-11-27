#pragma once

// [S2b] Сервіс погоди (Open-Meteo): пошук міста (geocoding), збереження місця окремо від Settings
// (NVS namespace "wx"), періодичне отримання поточної погоди й неблокуючий доступ до останніх даних.
//
// Старт: WeatherService::begin() викликається один раз у setup() ПІСЛЯ WifiManager::begin() і
// TimeService::begin(). Усі мережеві запити виконує ОДНА задача "weather" (ядро 0, пріоритет 1);
// жоден метод цього класу не робить мережевих викликів і не пише у flash у викликача: геттери беруть
// м'ютекс на кілька мікросекунд (таймаут weather_cfg::kLockWaitMs), команди лише ставлять запит у
// чергу / прапорець. Тому безпечні з будь-якої задачі, зокрема з обробників async_tcp й дисплея.
//
// Запити стартують лише коли Wi-Fi підключено, TimeService::isValid() і вистачає вільної внутрішньої
// RAM: [S2b2] для HTTP (за замовчуванням) — weather_cfg::kMinFreeHeapHttp / kMinLargestBlockHttp,
// для HTTPS — kMinFreeHeap / kMinLargestBlock. Транспорт — weather_cfg::kUseHttps (дефолт false).
// Місце «не задано» = немає запису в NVS. Settings і kFormatVersion не чіпаються.

#include <stddef.h>
#include <stdint.h>

#include "config/weather_config.h"
#include "core/weather_codes.h"

class WeatherService {
public:
    // Остання успішно отримана поточна погода.
    struct Current {
        float tempC;          // °C, округлено до 1 знака
        int16_t code;         // код WMO (weather::category())
        bool isDay;
        float windMs;         // м/с, 1 знак
        uint8_t humidity;     // %
        uint32_t fetchedAtMs; // millis() у момент отримання
    };

    // Вибране місце. name — UTF-8, до 31 байта.
    struct Location {
        bool set;
        char name[weather_cfg::kNameBytes];
        float lat;
        float lon;
    };

    // Кандидат geocoding.
    struct Candidate {
        char name[weather_cfg::kNameBytes];      // вже обрізано до 31 байта (як зберігається)
        char admin1[weather_cfg::kRegionBytes];  // область / регіон; порожньо, якщо немає
        char country[weather_cfg::kRegionBytes];
        float lat;
        float lon;
    };

    enum class SearchPhase : uint8_t { Idle, Searching, Done, NotFound, Error };

    struct SearchState {
        SearchPhase phase;
        char query[weather_cfg::kQueryMaxBytes + 1];  // останній прийнятий запит (після trim)
        uint8_t count;                                // 0..kMaxCities (лише при Done)
        Candidate results[weather_cfg::kMaxCities];
        char error[weather_cfg::kErrorBytes];         // код помилки при phase == Error, інакше ""
    };

    // Результат requestSearch().
    enum class SearchRequest : uint8_t {
        Accepted,     // запит у черзі, getSearchState() покаже Searching
        Busy,         // попередній пошук ще триває
        Empty,        // порожній запит (після обрізання пробілів)
        TooLong,      // довше kQueryMaxBytes
        BadChars,     // некоректний UTF-8 або керувальні символи
        Unavailable,  // сервіс не запущено або черга повна
    };

    // Консистентний знімок для GET /api/weather (одне взяття м'ютекса).
    struct Status {
        Location loc;
        bool valid;                              // є хоча б одна успішна відповідь для ЦЬОГО місця
        Current cur;                             // значущий лише при valid
        uint32_t ageMs;                          // kNoAge, якщо !valid
        char error[weather_cfg::kErrorBytes];    // код останньої помилки або ""
    };

    static constexpr uint32_t kNoAge = 0xFFFFFFFFu;

    // Створює м'ютекс, чергу, буфер відповіді (PSRAM), читає збережене місце з NVS і запускає задачу.
    // Повторний виклик безпечний. false — не вдалося створити ресурси / задачу.
    static bool begin();

    // Місце задано.
    static bool hasLocation();

    // Є дані для поточного місця (після зміни / очищення місця скидається до першого успіху).
    static bool hasData();

    // Копія останніх даних. false (out не змінюється), якщо даних немає або м'ютекс зайнято.
    static bool getCurrent(Current& out);

    // Мілісекунди від останнього успішного отримання; kNoAge, якщо даних немає.
    static uint32_t ageMs();

    // Назва вибраного міста ("" якщо не задано). Вказівник на внутрішній подвійний буфер: лишається
    // валідним і цілим, поки місце не змінять ДВІЧІ; для довгого використання копіюйте
    // (або getLocation()).
    static const char* cityName();

    // Копія вибраного місця. false, якщо м'ютекс зайнято.
    static bool getLocation(Location& out);

    // Консистентний знімок місця, даних і помилки. false, якщо м'ютекс зайнято.
    static bool getStatus(Status& out);

    // Ставить пошук міста в чергу (мережа — у задачі). query — UTF-8, 1..kQueryMaxBytes байт після
    // обрізання пробілів.
    static SearchRequest requestSearch(const char* query);

    // Копія стану пошуку (фаза, запит, кандидати). false, якщо м'ютекс зайнято.
    static bool getSearchState(SearchState& out);

    // Робить місцем кандидата останнього пошуку (index 0..count-1; лише при phase == Done).
    // false — немає результатів / index поза межами / сервіс не запущено.
    static bool selectCandidate(int index);

    // Пряме задання місця: name 1..31 байт UTF-8 без керувальних символів, lat -90..90, lon -180..180.
    // false — некоректні значення або сервіс не запущено. Запис у NVS робить задача (не викликач).
    static bool setLocation(const char* name, float lat, float lon);

    // Очищає місце й дані (NVS-запис видаляє задача). Результат поточного пошуку не чіпає.
    static void clearLocation();
};
