#pragma once

// Константи екрана «Список станцій» (Mode::Menu + MenuContext::StationList).
// [S2c2b] Створено: екран виводиться на ВСЮ панель 320×240 (DisplayManager::setFullPanel(true)), у стилі
// основного екрана та екрана EQ + Balance: шапка-смуга, область рядків на всю решту висоти, виділений
// рядок, позначка станції, що грає зараз. Поведінка (вибір, прокрутка, обрізання довгих назв) не змінена:
// вибраний рядок «прилипає» до нижнього рядка, коли індекс вибору виходить за kRows.
// Палітра береться з screens_cfg::main_layout; усі числа екрана — тут, screens.cpp литеральних координат
// і кольорів для цього екрана не містить.
// Розкладка (320×240):
//   y 0..23    шапка-смуга (колір смуги стану): «Станції» (Small) зліва
//   y 24       розділова лінія
//   y 28..235  8 рядків по 26 px (Small 17 px); виділений рядок — суцільна заливка на всю ширину
//   x 6..9     позначка станції, що грає зараз (вертикальна смужка бурштинового кольору)
//   x 18..312  назва станції (довга обрізається з "..." за наявним fitText)

#include <stdint.h>

#include "config/display_config.h"
#include "config/screens_config.h"

namespace station_list_cfg {

namespace ml = screens_cfg::main_layout;

constexpr int16_t kPanelW = display_cfg::kPanelW;   // 320
constexpr int16_t kPanelH = display_cfg::kPanelH;   // 240

// --- Тексти (українською; кирилиця в наявних шрифтах підтверджена S2c1 / S2c2a) ---
constexpr const char* kTitleText      = "Станції";
constexpr const char* kNoStationsText = "Немає станцій";

// --- Шапка ---
constexpr int16_t kHeaderH      = ml::kStatusH;                                  // 24: висота смуги
constexpr int16_t kHeaderPadX   = 8;                                             // бічний відступ заголовка
constexpr int16_t kHeaderTitleY = (kHeaderH - display_cfg::kFontSmallPx) / 2;    // Small, по центру смуги
constexpr int16_t kHeaderLineY  = kHeaderH;                                      // розділова лінія під шапкою

// --- Рядки ---
constexpr int16_t kRowsTop      = kHeaderLineY + 4;                              // 28: верх першого рядка
constexpr int16_t kRowH         = 26;
constexpr uint8_t kRows         = 8;                                             // видимих рядків (було 5)
constexpr int16_t kRowsBottom   = kRowsTop + kRows * kRowH;                      // 236: низ останнього рядка
constexpr int16_t kTextX        = 18;
constexpr int16_t kTextRightPad = ml::kSideMargin;
constexpr int16_t kTextDy       = (kRowH - display_cfg::kFontSmallPx) / 2;       // відступ тексту від верху рядка
constexpr int16_t kEmptyY       = (kPanelH - display_cfg::kFontLargePx) / 2;     // «Немає станцій» (Large), по центру

// --- Позначка станції, що грає зараз ---
constexpr int16_t kMarkX = 6;
constexpr int16_t kMarkW = 4;
constexpr int16_t kMarkH = 14;
constexpr int16_t kMarkDy = (kRowH - kMarkH) / 2;
// Радіо — логічний вхід 0 (специфікація, розділ 3): позначку показуємо лише коли активне радіо.
constexpr uint8_t kRadioInputIndex = 0;

// --- Кольори ---
constexpr uint16_t kColorBg          = ml::kColorBg;
constexpr uint16_t kColorHeaderBg    = ml::kColorStatusBg;
constexpr uint16_t kColorHeaderTitle = ml::kColorWxCity;
constexpr uint16_t kColorHeaderLine  = ml::kColorDivider;
constexpr uint16_t kColorSelectionBg = screens_cfg::kColorSelectionBg;           // виділений рядок (як було)
constexpr uint16_t kColorText        = ml::kColorWxCity;                         // звичайний рядок
constexpr uint16_t kColorTextSel     = display_cfg::kColorWhite;                 // виділений рядок
constexpr uint16_t kColorMark        = ml::kColorAmber;                          // позначка «грає зараз»
constexpr uint16_t kColorEmpty       = ml::kColorStatusText;                     // «Немає станцій»

static_assert(kRowsTop > kHeaderLineY, "rows must start below the header line");
static_assert(kRowsBottom <= kPanelH, "station list rows leave the panel");
static_assert(kRows >= 2, "station list must show at least two rows");
static_assert(kTextDy >= 0 && kTextDy + display_cfg::kFontSmallPx <= kRowH, "station name does not fit in its row");
static_assert(kMarkDy >= 0 && kMarkDy + kMarkH <= kRowH, "playing mark does not fit in its row");
static_assert(kMarkX + kMarkW < kTextX, "playing mark overlaps the station name");
static_assert(kPanelW - kTextX - kTextRightPad >= 100, "station name area too narrow");
static_assert(kHeaderTitleY >= 0, "header title must start on the screen");

}  // namespace station_list_cfg
