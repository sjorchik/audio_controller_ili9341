#pragma once

// Константи модуля audio/input_selector (реле-селектор зовнішніх входів).
// [Prompt 48a] Створено. Пін реле — лише в config/pins.h (pins::kInputRelay).
// [D2a] НОВИЙ ПРИСТРІЙ: полярність реле ПРОТИЛЕЖНА старому. Входи Computer (1) і Aux (2):
//   реле вимкнено (LOW)  = логічний вхід 1 (Computer),
//   реле увімкнено (HIGH) = логічний вхід 2 (Aux).
// Радіо (0) і Плеєр (3) реле НЕ чіпають. Рекомендація до плати (не код): pulldown на затворі / базі
// драйвера реле, щоб до begin() і під час скидання реле гарантовано було вимкнено (Computer).

#include <stdint.h>

#include "config/inputs_config.h"

namespace input_selector_cfg {

// true: рівень HIGH на піні = реле УВІМКНЕНО (Aux); false — інвертована схема (LOW = Aux).
// [D2a] Рішення власника: HIGH = Aux, LOW = Computer.
constexpr bool kRelayActiveHigh = true;

// Пауза після перемикання реле (контакти й аналоговий сигнал на PCM1808 заспокоюються), перед розмʼютом.
// Саму паузу тримає ВИКЛИКАЧ (AppController): модуль неблокуючий. Значення НЕ виміряне.
constexpr uint32_t kRelaySettleMs = 30;

// Логічні входи, які комутує реле ([D2a] беруться з inputs_cfg).
constexpr uint8_t kInputComputer = inputs_cfg::kComputer;  // реле вимкнено (LOW)
constexpr uint8_t kInputAux      = inputs_cfg::kAux;       // реле увімкнено (HIGH)

static_assert(kRelaySettleMs >= 1, "relay settle time must be >= 1 ms");
static_assert(kInputComputer != kInputAux && kInputComputer >= 1 && kInputAux >= 1,
              "relay inputs are external logical inputs (1..)");

}  // namespace input_selector_cfg
