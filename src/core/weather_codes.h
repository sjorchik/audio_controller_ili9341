#pragma once

// [S2b] Категорії погоди за кодами WMO (Open-Meteo `weather_code`). Лише логіка: іконки й рисування — S2c.

#include <stdint.h>

namespace weather {

enum class Condition : uint8_t {
    Clear,
    PartlyCloudy,
    Cloudy,
    Fog,
    Drizzle,
    Rain,
    Snow,
    Thunder,
    Unknown,
};

// Таблиця кодів WMO -> Condition. Припущення планування для кодів 0..3 (у специфікації вказано
// лише «1–3»): 0 і 1 («ясно» / «переважно ясно») -> Clear, 2 («мінлива хмарність») -> PartlyCloudy,
// 3 («суцільна хмарність») -> Cloudy. Змінюється в одному місці — тут.
//   45, 48 -> Fog; 51–57 -> Drizzle (з морозною мжичкою); 61–67 і 80–82 -> Rain (з крижаним дощем і
//   зливами); 71–77 і 85–86 -> Snow (з крупою й снігопадами); 95–99 -> Thunder.
// Параметр isDay зараз не впливає на категорію (день / ніч обирає іконку в S2c); лишено в
// сигнатурі, щоб майбутня розбивка (напр. «ясна ніч») не міняла виклики.
inline Condition category(int wmoCode, bool /*isDay*/) {
    switch (wmoCode) {
        case 0:
        case 1:
            return Condition::Clear;
        case 2:
            return Condition::PartlyCloudy;
        case 3:
            return Condition::Cloudy;
        case 45:
        case 48:
            return Condition::Fog;
        case 51: case 52: case 53: case 54: case 55: case 56: case 57:
            return Condition::Drizzle;
        case 61: case 62: case 63: case 64: case 65: case 66: case 67:
        case 80: case 81: case 82:
            return Condition::Rain;
        case 71: case 72: case 73: case 74: case 75: case 76: case 77:
        case 85: case 86:
            return Condition::Snow;
        case 95: case 96: case 97: case 98: case 99:
            return Condition::Thunder;
        default:
            return Condition::Unknown;
    }
}

// Стабільні імена для API (`condition` у GET /api/weather).
inline const char* conditionName(Condition c) {
    switch (c) {
        case Condition::Clear:        return "clear";
        case Condition::PartlyCloudy: return "partly_cloudy";
        case Condition::Cloudy:       return "cloudy";
        case Condition::Fog:          return "fog";
        case Condition::Drizzle:      return "drizzle";
        case Condition::Rain:         return "rain";
        case Condition::Snow:         return "snow";
        case Condition::Thunder:      return "thunder";
        case Condition::Unknown:      break;
    }
    return "unknown";
}

}  // namespace weather
