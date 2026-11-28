#pragma once

// Константи екрана «EQ + Balance» (режим редагування еквалайзера й балансу АКТИВНОГО входу).
// [Prompt 49a] Створено: таймаут, крок, початковий вибір, тимчасова текстова розкладка.
// [Prompt 49b] Тимчасова розкладка прибрана; додано справжню розкладку вертикальних стовпчиків
// (смуги EQ + баланс). Логіка (AppController) читає лише kTimeoutMs / kStepDb / kDefaultSelected —
// їх значення НЕ змінено.
// [E2a] Смуг EQ стало 8, [E4] — 10 (eq_cfg::kBandCount, чотири набори частот EQ_BAND_SET): колонок 11
// (10 смуг + баланс). Розкладку рахує ФОРМУЛА від ширини екрана й кількості колонок (kColW, kLeft, colX):
// зміна eq_cfg::kBandCount перебудовує екран сама. Підписи частот формуються в screens.cpp з
// eq_cfg::kBandFreqHz; значення («+18» / «-18») підбирають кегль і знак за шириною колонки (kValueSidePadPx).
// [S2c2a] Екран малюється на ВСЮ панель 320×240 (DisplayManager::setFullPanel(true)), у стилі основного
// екрана: палітра (смуга, роздільники, текст) береться з screens_cfg::main_layout. Додано шапку-смугу з
// назвою входу, рядок-«показник» вибраної смуги (частота + значення в дБ, XLarge) і вищі стовпчики.
// Решта константи (таймаут, крок, вибір) і назви публічних констант збережено. Усі числа екрана тут;
// screens.cpp не містить «магічних» координат.
// Розкладка (320×240):
//   y 0..23    шапка-смуга (колір смуги стану): «Еквалайзер» (Small) зліва, назва входу (Small) справа
//   y 24       розділова лінія
//   y 28..63   показник вибраного: підпис (частота / «Баланс», Large) зліва, значення в дБ (XLarge) справа
//   y 70..213  область стовпчиків (kAreaTop..kAreaBottom), нульова лінія при kZeroY = 142, піввисота 52 px
//   y 218..231 підписи частот і «Бал» (Tiny, kLabelY)

#include <stdint.h>

#include "config/balance_config.h"
#include "config/display_config.h"
#include "config/eq_config.h"
#include "config/screens_config.h"

namespace eq_screen_cfg {

namespace ml = screens_cfg::main_layout;

// Вихід з екрана без дій: від ОСТАННЬОЇ дії в екрані (UP / DOWN / LEFT / RIGHT / обертання / клік / VOL_*), мс.
constexpr uint32_t kTimeoutMs = 10000;

// Крок зміни вибраного параметра (смуга EQ і баланс), дБ на подію. UP / DOWN / обертання енкодера.
constexpr int kStepDb = 1;

// Параметр, вибраний при відкритті екрана: 0..kBandCount - 1 = смуги EQ, kBandCount = баланс.
constexpr uint8_t kDefaultSelected = 0;

// ---------------------------------------------------------------------------
// Панель й тексти ([S2c2a]; рядки українською — шрифти мають кирилицю)
// ---------------------------------------------------------------------------
constexpr int16_t kPanelW = display_cfg::kPanelW;   // 320
constexpr int16_t kPanelH = display_cfg::kPanelH;   // 240

constexpr const char* kTitleText      = "Еквалайзер";   // заголовок у шапці
constexpr const char* kBalanceLabel   = "Бал";           // підпис колонки балансу (Tiny)
constexpr const char* kBalanceCaption = "Баланс";        // підпис у показнику вибраного (Large)
constexpr const char* kUnitDb         = " дБ";           // після значення в показнику
constexpr const char* kUnitHz         = " Гц";           // після частоти < 1000 у показнику
constexpr const char* kUnitKhz        = " кГц";          // після частоти >= 1000 у показнику

// ---------------------------------------------------------------------------
// Шапка
// ---------------------------------------------------------------------------
constexpr int16_t kHeaderH        = ml::kStatusH;                                     // 24: висота смуги
constexpr int16_t kHeaderPadX     = 8;                                                // бічний відступ тексту шапки
constexpr int16_t kHeaderTitleY   = (kHeaderH - display_cfg::kFontSmallPx) / 2;       // Small: заголовок і назва входу (одна лінія)
constexpr int16_t kHeaderLineY    = kHeaderH;                                         // розділова лінія під шапкою
constexpr int16_t kHeaderNameGap  = 12;    // мінімальний проміжок між заголовком і назвою входу
constexpr uint16_t kColorBg          = ml::kColorBg;
constexpr uint16_t kColorHeaderBg    = ml::kColorStatusBg;
constexpr uint16_t kColorHeaderTitle = ml::kColorWxCity;
constexpr uint16_t kColorHeaderName  = display_cfg::kColorYellow;   // назва входу — акцент (що саме редагується)
constexpr uint16_t kColorHeaderLine  = ml::kColorDivider;

// ---------------------------------------------------------------------------
// Показник вибраного параметра
// ---------------------------------------------------------------------------
constexpr int16_t kReadoutY     = 28;                              // верх рядка (XLarge)
constexpr int16_t kReadoutH     = display_cfg::kFontXLargePx;      // висота рядка
constexpr int16_t kReadoutPadX  = 8;                               // бічні відступи
constexpr int16_t kReadoutCaptionY = kReadoutY + (kReadoutH - display_cfg::kFontLargePx) / 2;   // Large по центру рядка
constexpr int16_t kReadoutMinGap   = 8;                            // мінімум між підписом і значенням
constexpr uint16_t kColorReadoutCaption = ml::kColorWxCity;

// ---------------------------------------------------------------------------
// Колонки (kBandCount смуг EQ + баланс = kBandCount + 1)
// ---------------------------------------------------------------------------
constexpr uint8_t kColumnCount = eq_cfg::kBandCount + 1;
constexpr uint8_t kBalanceColumn = eq_cfg::kBandCount;

constexpr int16_t kSideMargin = 4;    // мінімальне поле зліва / справа від колонок
constexpr int16_t kGapEq    = 0;      // проміжок між смугами EQ (комірки впритул; бари розділяє поле навколо бару)
constexpr int16_t kGapBal   = 12;     // проміжок перед балансом (з розділювальною лінією посередині)
// Ширина комірки — з ширини панелі: (320 - 2·4 - 12 - 0) / 11 = 27 px при 10 смугах + баланс (kTotalW = 309, kLeft = 5).
constexpr int16_t kColW     = static_cast<int16_t>((kPanelW - 2 * kSideMargin - kGapBal -
                                                    (eq_cfg::kBandCount - 1) * kGapEq) / kColumnCount);
constexpr int16_t kBarW     = 20;     // ширина самого стовпчика (при kColW = 27 між барами лишається 7 px)
constexpr int16_t kTotalW   = kColumnCount * kColW + (eq_cfg::kBandCount - 1) * kGapEq + kGapBal;
constexpr int16_t kLeft     = (kPanelW - kTotalW) / 2;   // центрування по горизонталі

// Лівий край i-ї колонки. Перед балансом — (kBandCount - 1) проміжків EQ + kGapBal.
constexpr int16_t colX(uint8_t i) {
    return static_cast<int16_t>(kLeft + i * kColW +
                                (i < eq_cfg::kBandCount ? i * kGapEq
                                                        : (eq_cfg::kBandCount - 1) * kGapEq + kGapBal));
}

// Вертикальна розділова лінія між смугами EQ і балансом: по центру проміжку kGapBal.
constexpr int16_t kDividerX = static_cast<int16_t>(colX(kBalanceColumn) - kGapBal / 2);

// ---------------------------------------------------------------------------
// Вертикальна область і шкала
// ---------------------------------------------------------------------------
constexpr int16_t kAreaTop    = 70;                              // верх області стовпчиків
constexpr int16_t kAreaBottom = 214;                             // низ області (вище за підписи)
constexpr int16_t kZeroY      = (kAreaTop + kAreaBottom) / 2;    // нульова лінія (y = 142)
// Піввисота шкали: повна шкала (±kEqFullDb для EQ, ±kBalFullDb для балансу — однакова висота).
// Максимум, що вміщується разом із значенням Small над / під бару: 72 - 2 - 17 = 53; взято 52.
constexpr int16_t kHalfH      = 52;
constexpr int     kEqFullDb   = eq_cfg::kGainMaxDb;    // межа з eq_cfg (симетричний діапазон); висота бару = |дБ| / kEqFullDb × kHalfH
constexpr int     kBalFullDb  = balance_cfg::kMaxDb;   // 20

// Значення над / під стовпчиком: відступ від кінця бару, px.
constexpr int16_t kValueGap   = 2;
constexpr uint8_t kValueSmallPx = display_cfg::kFontSmallPx;   // вибране значення (жирніше за Tiny)
constexpr uint8_t kValueTinyPx  = display_cfg::kFontTinyPx;    // решта значень

// Підписи частот / «Бал» під областю.
constexpr int16_t kLabelY     = 218;

// Бічний запас (px, з кожного боку) між текстом значення й краєм колонки. Значення малюється Small (вибране) або
// Tiny (решта); якщо текст із запасом не влазить у kColW — кегль зменшується до Tiny, а якщо й так не влазить, додатне
// значення друкується без «+» (від'ємне — завжди зі знаком «-»; шрифт не має U+2212, тож мінус ASCII).
constexpr int16_t kValueSidePadPx = 1;

// ---------------------------------------------------------------------------
// Кольори
// ---------------------------------------------------------------------------
constexpr uint16_t kColorEqBar     = display_cfg::rgb565(90, 200, 255);   // EQ, вибране — холодний
constexpr uint16_t kColorEqBarDim  = display_cfg::rgb565(40, 80, 105);    // EQ, решта — приглушено
constexpr uint16_t kColorEqSel     = screens_cfg::kColorSelectionBg;     // підсвітка вибраної EQ-колонки
constexpr uint16_t kColorBalBar    = ml::kColorAmber;                    // баланс, вибране — бурштиновий (як на основному екрані)
constexpr uint16_t kColorBalBarDim = display_cfg::rgb565(110, 72, 0);     // баланс, решта
constexpr uint16_t kColorBalSel    = display_cfg::rgb565(80, 50, 0);      // підсвітка вибраної колонки балансу
constexpr uint16_t kColorBalValue  = display_cfg::rgb565(200, 140, 0);    // значення балансу, не вибране
constexpr uint16_t kColorZero      = screens_cfg::kColorDim;             // нульова лінія
constexpr uint16_t kColorDivider   = ml::kColorDivider;                  // розділювач EQ / Bal
constexpr uint16_t kColorValueDim  = screens_cfg::kColorDim;             // значення EQ, не вибране
constexpr uint16_t kColorValueSel  = display_cfg::kColorFg;              // значення вибраного
constexpr uint16_t kColorLabelDim  = screens_cfg::kColorDim;             // підписи, не вибрані
constexpr uint16_t kColorLabelSel  = display_cfg::kColorFg;              // підпис вибраного

// ---------------------------------------------------------------------------
// Перевірки розкладки (збій компіляції, якщо щось виходить за межі)
// ---------------------------------------------------------------------------
static_assert(kStepDb >= 1, "step must be at least 1 dB");
static_assert(kTimeoutMs >= 1000, "timeout is too short to use the screen");
static_assert(kDefaultSelected <= eq_cfg::kBandCount, "default selection out of range (0..bands = balance)");
static_assert(kHeaderLineY >= kHeaderTitleY + display_cfg::kFontSmallPx, "header line overlaps the title");
static_assert(kHeaderH >= display_cfg::kFontSmallPx, "header strip must hold the Small title");
static_assert(kReadoutY > kHeaderLineY, "readout starts above the header line");
static_assert(kReadoutY + kReadoutH < kAreaTop, "readout overlaps the columns area");
static_assert(kReadoutH >= display_cfg::kFontLargePx, "readout row must hold the Large caption");
static_assert(kAreaTop > kHeaderLineY, "columns area starts above the header line");
static_assert(kLeft >= 4 && kLeft + kTotalW <= kPanelW - 4, "columns do not fit the screen width");
static_assert(kBarW >= 2 && kBarW < kColW, "bar must be narrower than its column");
static_assert(kColW >= 24, "column is too narrow for the frequency labels / values (check eq_cfg::kBandCount)");
static_assert(kValueSidePadPx >= 0 && 2 * kValueSidePadPx < kColW, "value side padding must leave room inside the column");
static_assert(kGapBal >= 2, "gap before the balance column must fit the divider");
static_assert(kDividerX > colX(eq_cfg::kBandCount - 1) + kColW && kDividerX < colX(kBalanceColumn),
              "divider must sit inside the gap before the balance column");
static_assert(kHalfH > 0, "half height must be positive");
static_assert(kZeroY - kHalfH - kValueGap - kValueSmallPx >= kAreaTop,
              "top value label (selected, full scale) leaves the columns area");
static_assert(kZeroY + kHalfH + kValueGap + kValueSmallPx <= kAreaBottom,
              "bottom value label (selected, full scale) leaves the columns area");
static_assert(kAreaBottom + 2 <= kLabelY && kLabelY + display_cfg::kFontTinyPx <= kPanelH,
              "frequency labels leave the screen");
static_assert(kEqFullDb > 0 && kBalFullDb > 0, "full scale must be positive");
static_assert(kEqFullDb == -eq_cfg::kGainMinDb, "EQ column scale assumes a symmetric range (see eq_cfg::kGainRangeDb)");

}  // namespace eq_screen_cfg
