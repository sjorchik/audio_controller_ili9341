#pragma once

// [S2c1] Українські тексти основного екрана 320×240 (годинник, дата, погода, джерело).
// Лише дані: шрифти покривають кирилицю (ASCII / Latin-1 / Latin Ext-A / U+0400..U+045F, Ґ ґ).
// Символи «—» (U+2014) і «−» (U+2212) у шрифтах ВІДСУТНІ, тому мінус температури — ASCII '-'.
// «°» (U+00B0) і «·» (U+00B7) — це Latin-1, мають бути присутні; якщо на екрані їх немає —
// змінити kDegree / kSeparator нижче (наприклад, на "C" / "-"), нічого більше не чіпаючи.
// Мова написів основного екрана — рішення власника 2026-11-27 (макет затверджено «як є»).

#include <stddef.h>

namespace ua_text {

// Індекс = tm_wday (0 = неділя).
constexpr const char* kDayShort[7] = {"нд", "пн", "вт", "ср", "чт", "пт", "сб"};

// Родовий відмінок («27 листопада»); індекс = tm_mon (0 = січень).
constexpr const char* kMonthGen[12] = {
    "січня", "лютого", "березня", "квітня", "травня", "червня",
    "липня", "серпня", "вересня", "жовтня", "листопада", "грудня",
};

// Індекс = static_cast<size_t>(weather::Condition): Clear, PartlyCloudy, Cloudy, Fog, Drizzle,
// Rain, Snow, Thunder, Unknown (порядок enum у core/weather_codes.h; розмір перевіряє screens.cpp).
constexpr const char* kCondition[] = {
    "Ясно", "Мінлива хмарність", "Хмарно", "Туман", "Мряка", "Дощ", "Сніг", "Гроза", "Невідомо",
};
constexpr size_t kConditionCount = sizeof(kCondition) / sizeof(kCondition[0]);

constexpr const char* kClockUnknown  = "--:--";           // час недійсний
constexpr const char* kTimeUnknown   = "Час невідомий";   // у рядку дати
constexpr const char* kCityNotSet    = "Місто не задано";
constexpr const char* kNoWeatherData = "Немає даних";     // місто задано, даних ще немає
constexpr const char* kTempUnknown   = "--";              // перед kDegree
constexpr const char* kDegree        = "°";
constexpr const char* kUnitWind   = "м/с";
constexpr const char* kUnitPressure   = "мм";   // [S2b3] екран показує мм рт. ст. (API — гПа)
constexpr const char* kPressureUnknown = "---";   // немає даних про тиск
constexpr const char* kSeparator     = " · ";             // зарезервовано (рядок «Місто · стан» розбито на два)

constexpr const char* kSourceExternal = "Вхід";           // тег джерела для Computer / Aux / Player
constexpr const char* kNoLink         = "Немає зв'язку";  // Player (заглушка до інтеграції media-модуля)

}  // namespace ua_text
