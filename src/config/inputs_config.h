#pragma once

// Логічні входи нового пристрою (ILI9341). [D2a] Створено.
// Порядок (рішення власника 2026-11-24): 0 = Радіо, 1 = Computer, 2 = Aux, 3 = Плеєр.
//  - Радіо: цифровий шлях (AudioPlayer), реле не чіпає.
//  - Computer / Aux: аналогові входи через ОДНЕ DPDT-реле (audio/input_selector) і прохід PCM1808 -> line_path.
//  - Плеєр: заглушка до інтеграції media-модуля; проходу line_path немає, реле не чіпає, вихід у тиші.
// Єдине місце, яке відрізняє «вхід із проходом» від решти: usesLinePath().

#include <stdint.h>

#include "config/defaults.h"

namespace inputs_cfg {

constexpr uint8_t kRadio    = 0;
constexpr uint8_t kComputer = 1;
constexpr uint8_t kAux      = 2;
constexpr uint8_t kPlayer   = 3;

// Входи, що йдуть через реле й line_path (послідовний діапазон).
constexpr uint8_t kFirstLine = kComputer;
constexpr uint8_t kLastLine  = kAux;

constexpr bool usesLinePath(uint8_t idx) {
    return idx >= kFirstLine && idx <= kLastLine;
}

static_assert(kPlayer + 1 == defaults::kInputCount, "inputs_cfg must list exactly defaults::kInputCount inputs");
static_assert(kRadio == 0, "input 0 is the radio (always enabled, see settings_cfg::kInputMaskRadioBit)");
static_assert(kFirstLine <= kLastLine && kLastLine < kPlayer, "line_path inputs precede the player stub");

}  // namespace inputs_cfg
