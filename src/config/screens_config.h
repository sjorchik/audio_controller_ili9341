#pragma once

// Константи модуля ui/screens (розкладка 320×170, кольори, marquee, standby).
// [S2c2b] Спливні вікна гучності / балансу ПРИБРАНО (рішення власника); список станцій —
// config/station_list_config.h; екран EQ + Balance — config/eq_screen_config.h.
// [S2c1] Основний екран (Radio / ExternalInput) — namespace screens_cfg::main_layout (320×240);
// константи Radio / ExternalInput / VU / верхньої смуги вище (kTop*, kNet*, kStation*, kTrack*, kVu*,
// kExt*) з S2c1 НЕ використовуються й лишені для сумісності static_assert (прибрати в S2c2).
// Шрифти: XLarge 36 px (назва станції P35, назва зовнішнього входу P38), Large 24 px, Small 17 px, Tiny 14 px; іконка 24×24
// (display_config.h).

#include <stdint.h>

#include "config/defaults.h"  // [Prompt 46b] kInputCount (static_assert кольорів входів)
#include "config/display_config.h"
#include "config/vu_config.h"

namespace screens_cfg {

constexpr int16_t kW = display_cfg::kWidth;
// [D1] Область екранів = верхні 320×kLegacyViewportH панелі ILI9341 320×240 (решта чорна).
constexpr int16_t kH = display_cfg::kLegacyViewportH;

// --- Загальне ---
constexpr int16_t kMargin   = 8;                  // бічні поля
constexpr int16_t kContentW = kW - 2 * kMargin;   // ширина області marquee/VU

// --- Кольори ---
constexpr uint16_t kColorAccent      = display_cfg::kColorYellow;
constexpr uint16_t kColorDim         = display_cfg::kColorGray;
// [Prompt 35] Метадані треку: світло-сірий (світліший за kColorDim, але не білий — ієрархія з
// назвою станції, що білою). Колір статусів потоку (statusText) цієї константи НЕ використовує.
constexpr uint16_t kColorTrack       = display_cfg::rgb565(210, 210, 210);
constexpr uint16_t kColorOk          = display_cfg::kColorGreen;
constexpr uint16_t kColorBad         = display_cfg::kColorRed;
constexpr uint16_t kColorSelectionBg = display_cfg::rgb565(0, 70, 150);

// --- Standby ---
constexpr uint8_t kStandbyBrightnessPercent = 5;   // див. обґрунтування
constexpr int16_t kStandbyIconX    = (kW - display_cfg::kIconSize) / 2;
constexpr int16_t kStandbyIconY    = 73;
constexpr int16_t kStandbyCaptionY = 105;          // Tiny, по центру

// --- Верхній рядок (Radio, ExternalInput): іконки Wi-Fi / мʼюту ---
// [Prompt 33] Назву входу з екрана Radio прибрано: kTopBarTextY і kTopBarNameGap видалено.
constexpr int16_t kTopBarY     = 4;
constexpr int16_t kWifiIconX   = kW - kMargin - display_cfg::kIconSize;
constexpr int16_t kMuteIconX   = kWifiIconX - display_cfg::kIconSize - 6;
constexpr int16_t kOfflineMuteGap = 6;              // [Prompt 28] проміжок між позначкою "offline" і іконкою мʼюту

// --- [Prompt 36] Мережа Wi-Fi у смузі іконок екрана Radio (ліворуч від іконок) ---
// Один рядок FontSize::Tiny: "SSID  IP", по вертикалі в рівень іконок. Якщо не вміщується, обрізається
// SSID (fitText() з "..."), IP лишається цілим. Ширина смуги рахується від іконки мʼюту (найлівіша зі
// стаціонарних іконок), тож розкладка не стрибає, коли мʼют вмикається. Офлайн / AP — смуга порожня.
constexpr int16_t  kNetX    = kMargin;
constexpr int16_t  kNetY    = kTopBarY + (display_cfg::kIconSize - display_cfg::kFontTinyPx) / 2;  // верх рядка
constexpr int16_t  kNetGap  = 6;                                   // відступ від іконки мʼюту
constexpr int16_t  kNetTextGap = 10;                               // між SSID і IP
constexpr int16_t  kNetW    = kMuteIconX - kNetGap - kNetX;        // ширина смуги тексту
constexpr uint16_t kColorNet = display_cfg::rgb565(160, 160, 160); // трохи світліше за kColorDim
// Іконка Wi-Fi за RSSI (дБм): >= kWifiRssiBar3Db -> Wifi3, >= kWifiRssiBar2Db -> Wifi2, інакше Wifi1
// (WifiOff лише без звʼязку). Гістерезис: перехід вгору при порозі + kWifiRssiHystDb/2, вниз при
// порозі - kWifiRssiHystDb/2.
constexpr int8_t   kWifiRssiBar3Db  = -60;
constexpr int8_t   kWifiRssiBar2Db  = -72;
constexpr int8_t   kWifiRssiHystDb  = 4;

// --- Radio ---
// [Prompt 35] Назва станції — FontSize::XLarge (36 px), marquee; одразу під рядком іконок
// (kTopBarY + kIconSize), тож іконки Wi-Fi / мʼюту / "offline" не перетинаються з назвою.
// Нижче — обʼєднаний рядок FontSize::Large. Усе вертикально вкладається над VU із запасом kVuGapAbove.
constexpr int16_t kStationY   = kTopBarY + display_cfg::kIconSize;         // XLarge, marquee (y=28)
constexpr int16_t kStationGap = 2;                                         // між назвою й рядком інфо
// [Prompt 33] Обʼєднаний рядок: метадані (kColorTrack) АБО статус потоку (свій колір), marquee.
// [Prompt 35] Шрифт Large (раніше Small). kStateY (старий окремий рядок статусу) прибрано.
constexpr int16_t kTrackY     = kStationY + display_cfg::kFontXLargePx + kStationGap;   // y=66
constexpr int16_t kVuGapAbove = 4;                 // мінімальний проміжок між рядком інфо й VU
// --- [Prompt 34] VU у стилі індикатора «Маяк-233» (ВЛЛ): дві сегментні смуги L (верх) / R (низ),
// між ними нерухома шкала в дБ. Мапінг рівня на сегменти лінійний за дБ (vu_cfg::kDbFloor..0).
// Смуги від лівого поля, підписи L/R праворуч від них; усе виводиться з констант нижче.
constexpr int16_t  kVuX          = kMargin;          // зона блоку (лише для ENABLE_VU == 0: рамка)
constexpr int16_t  kVuY          = 94;               // верх верхньої смуги (L)
constexpr int16_t  kVuW          = kContentW;
constexpr uint8_t  kVuSegments   = 41;               // сегментів на смугу
constexpr int16_t  kVuSegW       = 5;
constexpr int16_t  kVuSegGap     = 2;
constexpr int16_t  kVuSegPitch   = kVuSegW + kVuSegGap;
constexpr int16_t  kVuUsedW      = kVuSegments * kVuSegPitch - kVuSegGap;   // ширина смуги
constexpr int16_t  kVuBarX       = kMargin;          // лівий край смуг
constexpr int16_t  kVuLabelGap   = 6;                // між смугою і підписом L / R
constexpr int16_t  kVuLabelX     = kVuBarX + kVuUsedW + kVuLabelGap;   // підписи L / R (Tiny) праворуч
constexpr int16_t  kVuBarH       = 18;
constexpr int16_t  kVuRowGap     = 3;                // між смугою і рядком шкали
constexpr int16_t  kVuLY         = kVuY;
constexpr int16_t  kVuScaleY     = kVuLY + kVuBarH + kVuRowGap;                         // Tiny, підписи дБ
constexpr int16_t  kVuRY         = kVuScaleY + display_cfg::kFontTinyPx + kVuRowGap;
constexpr int16_t  kVuH          = kVuRY + kVuBarH - kVuY;                              // висота всього блоку
// Зона перевантаження: сегменти з номера kVuRedSeg (0-based) до кінця; поріг = vu_cfg::kRedFrom.
constexpr uint8_t  kVuRedSeg     = static_cast<uint8_t>(vu_cfg::kRedFrom * kVuSegments + 0.5f);
// Базовий рівень: у повній тиші (і без даних) на екрані Radio завжди світяться перші сегменти, як на ВЛЛ.
constexpr uint8_t  kVuMinSegments = 2;
// Утримання піку: один сегмент лишається яскравим kVuPeakHoldMs, далі спадає на сегмент за kVuPeakFallMs.
constexpr uint32_t kVuPeakHoldMs = 100;
constexpr uint32_t kVuPeakFallMs = 60;
// Мітки шкали, дБFS: мусять лежати в (vu_cfg::kDbFloor, 0] і зростати. Позиція = (дБ - kDbFloor) / (-kDbFloor).
constexpr int8_t   kVuScaleMarksDb[] = {-20, -14, -9, -5, -2, 0};
constexpr uint8_t  kVuScaleMarkCount = sizeof(kVuScaleMarksDb) / sizeof(kVuScaleMarksDb[0]);
constexpr bool vuMarksValid() {
    for (uint8_t i = 0; i < kVuScaleMarkCount; ++i) {
        if (kVuScaleMarksDb[i] <= vu_cfg::kDbFloor || kVuScaleMarksDb[i] > 0) return false;
        if (i > 0 && kVuScaleMarksDb[i] <= kVuScaleMarksDb[i - 1]) return false;
    }
    return true;
}
// Кольори: бірюзовий люмінофор ВЛЛ (за фото), перевантаження — червоний; непідсвічені — приглушені
// відтінки тих самих кольорів.
constexpr uint16_t kColorVuLit      = display_cfg::rgb565(0, 235, 215);
constexpr uint16_t kColorVuOver     = display_cfg::rgb565(255, 50, 40);
constexpr uint16_t kColorVuOff      = display_cfg::rgb565(10, 44, 46);
constexpr uint16_t kColorVuOverOff  = display_cfg::rgb565(52, 14, 12);
constexpr uint16_t kColorVuScale    = kColorVuLit;   // усі підписи VU того ж бірюзового, що й підсвічені сегменти

// --- ExternalInput ---
// [Prompt 38] Напис "INPUT" прибрано. Назва входу (FontSize::XLarge) центрується по горизонталі
// (в межах kContentW) і по вертикалі у вільній області між смугою іконок і блоком VU.
constexpr int16_t kExtAreaTop    = kTopBarY + display_cfg::kIconSize;   // низ смуги іконок (y=28)
constexpr int16_t kExtAreaBottom = kVuY - kVuGapAbove;                  // верх VU мінус запас (y=90)
constexpr int16_t kExtNameDy     = -4;   // ручна підгонка: від'ємне = вище (P38, за побажанням власника)
constexpr int16_t kExtNameY      = kExtAreaTop +
    (kExtAreaBottom - kExtAreaTop - display_cfg::kFontXLargePx) / 2 + kExtNameDy;   // верх рядка шрифту (y=33)
// [D2a] Світлі кольори назви за номером не-радіо входу 1..3 (індекс масиву = inputIndex - 1):
// Computer, Aux, Плеєр. Підібрано, щоб відрізнятись від бірюзового VU (kColorVuLit = 0,235,215): червоний і
// зелений — без синьої складової; Плеєр — світло-фіолетовий (синя складова є, але з сильною червоною — на тлі
// бірюзового VU не плутається).
constexpr uint16_t kColorInputName[] = {
    display_cfg::rgb565(255, 120, 120),   // вхід 1 (Computer) — світло-червоний
    display_cfg::rgb565(120, 255, 120),   // вхід 2 (Aux) — світло-зелений
    display_cfg::rgb565(200, 150, 255),   // вхід 3 (Плеєр) — світло-фіолетовий
};
static_assert(sizeof(kColorInputName) / sizeof(kColorInputName[0]) + 1 == defaults::kInputCount,
              "one name color per external input (logical 1..kInputCount-1)");
constexpr uint8_t kColorInputNameCount = sizeof(kColorInputName) / sizeof(kColorInputName[0]);

// --- Статус-рядок (Radio і ExternalInput) ---
constexpr int16_t kStatusLineY = 143;
constexpr int16_t kStatusY     = 148;              // Small: ціль зліва, значення справа

// --- [Prompt 15] IrLearn: статус (Small) над назвою дії (Large), підказка (Small) під нею ---
constexpr int16_t kIrStatusY = 24;
constexpr int16_t kIrActionY = 62;
constexpr int16_t kIrHintY   = 108;

// --- [Prompt 16] OtaUpdate: заголовок Large, відсоток Large, смуга, підказка Small ---
constexpr int16_t kOtaTitleY   = 28;
constexpr int16_t kOtaPercentY = 64;
constexpr int16_t kOtaBarX     = kMargin;
constexpr int16_t kOtaBarY     = 106;
constexpr int16_t kOtaBarW     = kContentW;
constexpr int16_t kOtaBarH     = 16;
constexpr int16_t kOtaHintY    = 136;

// --- Плейсхолдер (Menu без контексту) ---
constexpr int16_t kPlaceholderY = 73;              // Large, по центру

// --- [Prompt 12] WifiSetup: заголовок Large, під ним SSID і адреса Small ---
constexpr int16_t kWifiSetupTitleY = 48;
constexpr int16_t kWifiSetupSsidY  = 88;
constexpr int16_t kWifiSetupIpY    = 112;
constexpr int16_t kWifiSetupHintY  = 140;   // [Prompt 28] Small: підказка "OK: work offline"

// --- Marquee ---
constexpr uint32_t kMarqueeStepMs      = 40;       // крок зсуву
constexpr int16_t  kMarqueeStepPx      = 2;        // 50 px/с
constexpr uint32_t kMarqueeEdgePauseMs = 1500;     // пауза на початку й у кінці

static_assert(kStatusY + display_cfg::kFontSmallPx <= kH, "status row out of screen");
static_assert(kVuY + kVuH <= kH - 4, "VU block leaves the screen");
static_assert(kTrackY + display_cfg::kFontLargePx + kVuGapAbove <= kVuY, "info line overlaps VU frame");
static_assert(kStationY + display_cfg::kFontXLargePx < kTrackY, "station overlaps info line");
static_assert(kStationY >= kTopBarY + display_cfg::kIconSize, "station name overlaps the top icons row");
static_assert(kExtNameY >= kExtAreaTop && kExtNameY + display_cfg::kFontXLargePx <= kExtAreaBottom,
              "ExternalInput name must fit between the icons row and the VU block");
static_assert(kMuteIconX > kMargin, "icons overflow top bar");
static_assert(kNetY >= 0 && kNetY + display_cfg::kFontTinyPx <= kStationY,
              "Wi-Fi info row overlaps the station name or leaves the screen");
static_assert(kNetX + kNetW + kNetGap == kMuteIconX && kNetW >= 100, "Wi-Fi info strip too narrow");
static_assert(kWifiRssiBar3Db > kWifiRssiBar2Db && kWifiRssiHystDb >= 0 &&
                  kWifiRssiBar3Db - kWifiRssiBar2Db > kWifiRssiHystDb,
              "Wi-Fi RSSI thresholds must be descending and wider than the hysteresis");
static_assert(kWifiSetupTitleY + display_cfg::kFontLargePx < kWifiSetupSsidY &&
                  kWifiSetupSsidY + display_cfg::kFontSmallPx < kWifiSetupIpY &&
                  kWifiSetupIpY + display_cfg::kFontSmallPx <= kH,
              "WifiSetup layout overlaps or leaves the screen");
static_assert(kWifiSetupIpY + display_cfg::kFontSmallPx < kWifiSetupHintY &&
                  kWifiSetupHintY + display_cfg::kFontSmallPx <= kH,
              "WifiSetup offline hint overlaps the address or leaves the screen");
static_assert(kIrStatusY + display_cfg::kFontSmallPx < kIrActionY &&
                  kIrActionY + display_cfg::kFontLargePx < kIrHintY &&
                  kIrHintY + display_cfg::kFontSmallPx <= kH,
              "IrLearn layout overlaps or leaves the screen");
static_assert(kOtaTitleY + display_cfg::kFontLargePx < kOtaPercentY &&
                  kOtaPercentY + display_cfg::kFontLargePx < kOtaBarY &&
                  kOtaBarY + kOtaBarH < kOtaHintY &&
                  kOtaHintY + display_cfg::kFontSmallPx <= kH,
              "OtaUpdate layout overlaps or leaves the screen");
static_assert(kMarqueeStepMs > 0 && kMarqueeStepPx > 0, "marquee step must be positive");
static_assert(kVuSegW >= 2 && kVuSegGap >= 1 && kVuLabelX + 12 <= kW - 4,
              "VU bars do not fit into the screen width");
static_assert(kVuMinSegments < kVuRedSeg, "VU base level must stay below the overload zone");
static_assert(kVuRedSeg > 0 && kVuRedSeg < kVuSegments, "VU overload segment out of range");
static_assert(kVuPeakHoldMs > 0 && kVuPeakFallMs > 0, "VU peak timings must be positive");
static_assert(kVuScaleMarkCount > 0 && vuMarksValid(), "VU scale marks must lie in (kDbFloor, 0] ascending");

// ---------------------------------------------------------------------------
// [S2c1] ОСНОВНИЙ ЕКРАН 320×240 (Radio / ExternalInput, включно з Computer / Aux / Player).
// Малюється на ВСЮ панель (display_cfg::kPanelH рядків); решта екранів лишаються у верхніх
// kLegacyViewportH рядках до S2c2. Усі координати — тут, у screens.cpp литеральних немає.
// Вертикальна карта (y, пікселів):
//   0..23    смуга стану (іконка Wi-Fi, SSID ліворуч, IP праворуч)
//   28..100  блок годинника (ліворуч) і погоди (праворуч), вертикальний роздільник
//   104      горизонтальний роздільник
//   109..123 тег джерела (Tiny) · 125..161 назва станції / входу (XLarge)
//   163..180 рядок треку / «Немає зв'язку» (Small)
//   189..231 два рядки VU (L / R) і число гучності (XLarge) праворуч
// ---------------------------------------------------------------------------
namespace main_layout {

constexpr int16_t kPanelW     = display_cfg::kPanelW;    // 320
constexpr int16_t kPanelH     = display_cfg::kPanelH;    // 240
constexpr int16_t kSideMargin = 8;
constexpr int16_t kRightX     = kPanelW - kSideMargin;   // 312: права межа контенту
constexpr int16_t kContentW   = kPanelW - 2 * kSideMargin;   // 304: ширина marquee (== screens_cfg::kContentW)

// Індекс входу «Плеєр» (= inputs_cfg::kPlayer зі спеки D2a; тут через defaults::kInputCount, бо
// inputs_config.h не прикріплено). Радіо = 0.
constexpr uint8_t kPlayerInputIndex = static_cast<uint8_t>(defaults::kInputCount - 1);

// --- Оновлення / продуктивність ---
// true: якщо змінились лише VU і / або marquee (решта кадру ідентична попередньому), на панель
// передаються лише рядки, що містять ці елементи (передача 320×240 ≈ 31 мс при 40 МГц проти періоду
// кадру 40 мс; смуга VU 42 рядки ≈ 5,4 мс). false: завжди всі 240 рядків.
constexpr bool     kPartialPush    = true;
constexpr uint32_t kClockPollMs    = 250;      // як часто перечитувати час (getLocal)
constexpr uint32_t kWeatherPollMs  = 1000;     // як часто брати знімок погоди (getStatus)
constexpr uint32_t kWeatherStaleMs = 2UL * 60UL * 60UL * 1000UL;   // дані старші — приглушений колір

// --- Кольори ---
constexpr uint16_t kColorBg         = display_cfg::kColorBlack;
constexpr uint16_t kColorStatusBg   = display_cfg::rgb565(22, 25, 34);
constexpr uint16_t kColorStatusText = display_cfg::rgb565(160, 160, 160);
constexpr uint16_t kColorDivider    = display_cfg::rgb565(48, 52, 64);
constexpr uint16_t kColorAmber      = display_cfg::rgb565(255, 176, 0);   // бурштиновий
constexpr uint16_t kColorClock      = display_cfg::kColorWhite;
constexpr uint16_t kColorClockDim   = display_cfg::rgb565(110, 110, 118); // «--:--»
constexpr uint16_t kColorDate       = display_cfg::rgb565(190, 190, 200);
constexpr uint16_t kColorTemp       = display_cfg::kColorWhite;
constexpr uint16_t kColorWxCity     = display_cfg::rgb565(200, 200, 210);
constexpr uint16_t kColorWxState    = display_cfg::rgb565(150, 155, 170);
constexpr uint16_t kColorWxStale    = display_cfg::rgb565(95, 95, 100);   // застарілі / відсутні дані
constexpr uint16_t kColorTag        = display_cfg::rgb565(110, 170, 255);
constexpr uint16_t kColorStation    = display_cfg::kColorWhite;
constexpr uint16_t kColorVolume     = display_cfg::kColorWhite;
constexpr uint16_t kColorVolumeMute = display_cfg::kColorGray;
// Іконки погоди (малюються з примітивів).
constexpr uint16_t kColorSun        = display_cfg::rgb565(255, 200, 0);
constexpr uint16_t kColorMoon       = display_cfg::rgb565(225, 225, 190);
constexpr uint16_t kColorCloudLight = display_cfg::rgb565(205, 210, 222);
constexpr uint16_t kColorCloudDark  = display_cfg::rgb565(140, 146, 160);
constexpr uint16_t kColorRain       = display_cfg::rgb565(80, 150, 255);
constexpr uint16_t kColorSnow       = display_cfg::kColorWhite;
constexpr uint16_t kColorBolt       = display_cfg::rgb565(255, 220, 0);
constexpr uint16_t kColorFog        = display_cfg::rgb565(170, 176, 190);

// --- Смуга стану ---
// Іконки Wi-Fi 24×24 мають «чорнило» в рядках 2..21 (виміряно за масками icons.cpp), тож смуга 24.
constexpr int16_t kStatusH     = 24;
constexpr int16_t kStatusIconX = 4;
constexpr int16_t kStatusIconY = 0;
constexpr int16_t kStatusTextX = kStatusIconX + display_cfg::kIconSize + 2;
constexpr int16_t kStatusTextY = (kStatusH - display_cfg::kFontTinyPx) / 2;
constexpr int16_t kStatusGap   = 10;     // між SSID і IP

// --- Годинник (FontSize::Clock; верх цифр, а не верх рядка шрифту) ---
constexpr int16_t kClockX            = kSideMargin;
constexpr int16_t kClockDigitTopY    = 33;    // верх цифр
constexpr int16_t kClockDigitH       = 36;    // макс. висота цифри (S2c0: 34..36)
constexpr int16_t kClockFontTopInset = 10;    // верх рядка шрифту -> верх цифри (S2c0: 9..10, Pillow; ВИМІРЯТИ)
constexpr int16_t kClockDy           = 12;    // виміряно на залізі (фото 2026-11-28): цифри стояли на y~17 замість 29
constexpr int16_t kClockFontY        = kClockDigitTopY - kClockFontTopInset + kClockDy;
constexpr int16_t kDateY             = kClockDigitTopY + kClockDigitH + 8;   // Small, під цифрами
constexpr int16_t kSepX              = 154;   // вертикальний роздільник годинник | погода
constexpr int16_t kClockBlockW       = kSepX - kSideMargin - 4;              // ширина рядка дати
constexpr int16_t kClockCenterX       = (kSideMargin + kSepX) / 2;   // вісь блоку годинника

// --- Погода ---
constexpr int16_t kWeatherX        = 162;
constexpr int16_t kWeatherW        = kRightX - kWeatherX;
constexpr int16_t kWeatherIconSize = 36;
constexpr int16_t kBlockTop        = kStatusH + 4;
constexpr int16_t kCityY           = kBlockTop;                                    // Small, верх блоку
constexpr int16_t kWeatherIconY    = kCityY + display_cfg::kFontSmallPx + 1;       // рядок «іконка | стан | температура»
constexpr int16_t kTempY           = kWeatherIconY;                                // XLarge, прижата до kRightX
constexpr int16_t kStateY          = kWeatherIconY + kWeatherIconSize + 4;         // нижній рядок Tiny: вітер, вологість, тиск
constexpr int16_t kBlockBottom     = kStateY + display_cfg::kFontTinyPx;
constexpr int16_t kDividerY        = kBlockBottom + 4;

// --- Джерело, назва, рядок треку ---
constexpr int16_t kTagY     = kDividerY + 5;                                   // Tiny
constexpr int16_t kStationY = kTagY + display_cfg::kFontTinyPx + 2;            // XLarge
constexpr int16_t kTrackY   = kStationY + display_cfg::kFontXLargePx + 2;      // Small

// --- VU (два рядки) і гучність ---
constexpr int16_t  kVuLabelX    = kSideMargin;                 // «L» / «R» (Tiny)
constexpr int16_t  kVuBarX      = 22;
constexpr uint8_t  kVuSegments  = 27;
constexpr int16_t  kVuSegW      = 5;
constexpr int16_t  kVuSegGap    = 1;
constexpr int16_t  kVuSegPitch  = kVuSegW + kVuSegGap;
constexpr int16_t  kVuUsedW     = kVuSegments * kVuSegPitch - kVuSegGap;
constexpr int16_t  kVuBarH      = 18;
constexpr int16_t  kVuRowGap    = 6;
constexpr int16_t  kVuLY        = 189;
constexpr int16_t  kVuRY        = kVuLY + kVuBarH + kVuRowGap;
constexpr int16_t  kVuBandH     = kVuRY + kVuBarH - kVuLY;     // рядки, що передаються при «лише VU»
// Пороги кольору: зелений до kVuAmberFromDb, бурштиновий до vu_cfg::kRedFrom, далі червоний.
// ПРИМІТКА: vu_cfg::kYellowFrom (0,70 ≈ −6 дБ) тут не використовується: за макетом власника зелений до ≈ −9 дБ.
constexpr float    kVuAmberFromDb = -9.0f;
constexpr uint8_t  kVuAmberSeg  = static_cast<uint8_t>(
    (kVuAmberFromDb - vu_cfg::kDbFloor) / (-vu_cfg::kDbFloor) * kVuSegments + 0.5f);
constexpr uint8_t  kVuRedSeg    = static_cast<uint8_t>(vu_cfg::kRedFrom * kVuSegments + 0.5f);
constexpr uint8_t  kVuMinSegments = 2;                         // базовий рівень Radio / Computer / Aux (не Player)
constexpr uint32_t kVuPeakHoldMs  = 100;
constexpr uint32_t kVuPeakFallMs  = 60;
constexpr uint16_t kColorVuGreen    = display_cfg::rgb565(0, 215, 70);
constexpr uint16_t kColorVuGreenOff = display_cfg::rgb565(6, 40, 18);
constexpr uint16_t kColorVuAmber    = kColorAmber;
constexpr uint16_t kColorVuAmberOff = display_cfg::rgb565(52, 36, 0);
constexpr uint16_t kColorVuRed      = display_cfg::rgb565(255, 50, 40);
constexpr uint16_t kColorVuRedOff   = display_cfg::rgb565(52, 14, 12);
constexpr uint16_t kColorVuLabel    = display_cfg::rgb565(150, 160, 170);
// Число гучності (XLarge, праворуч) і іконка мʼюту ліворуч від нього (лише коли мʼют).
constexpr int16_t  kVolY         = kVuLY + (kVuBandH - display_cfg::kFontXLargePx) / 2;
constexpr int16_t  kIconStripPitch = display_cfg::kIconSize + 4;
constexpr int16_t  kVolMuteIconX = kRightX - display_cfg::kIconSize;   // слот 0 (крайній правий)
constexpr int16_t  kVolMuteIconY = kTagY - 4;
constexpr int16_t  kVolLabelGap = 8;   // між «Vol» і числом
constexpr int16_t  kWindArrowW  = 12;   // ширина слоту стрілки напрямку вітру
constexpr int16_t  kWindArrowR  = 5;    // півдовжина стрілки

static_assert(kContentW == screens_cfg::kContentW, "marquee width must match screens_cfg::kContentW");
static_assert(defaults::kInputCount >= 2, "player input index must be an external input");
static_assert(kStatusH >= display_cfg::kIconSize, "status bar must hold the 24 px Wi-Fi icon");
static_assert(kClockFontY >= 0, "clock font line must start on the screen");
static_assert(kDateY + display_cfg::kFontSmallPx < kDividerY, "date overlaps the divider");
static_assert(kClockBlockW > 100, "date row too narrow");
static_assert(kWeatherX > kSepX + 4 && kWeatherW >= 120, "weather block too narrow");
static_assert(kStateY + display_cfg::kFontTinyPx < kDividerY, "weather state row overlaps the divider");
static_assert(kTrackY + display_cfg::kFontSmallPx + 4 <= kVuLY, "track row overlaps the VU block");
static_assert(kVuRY + kVuBarH + 4 <= kPanelH, "VU block leaves the screen");
static_assert(kVolMuteIconY >= kDividerY && kVolMuteIconY + 20 <= kStationY, "icon strip must sit in the tag row");
static_assert(kVuBarX + kVuUsedW + 8 <= kRightX - 3 * 19 - kVolLabelGap - 52, "VU bars collide with the Vol label");
static_assert(kVuAmberSeg > kVuMinSegments && kVuAmberSeg < kVuRedSeg && kVuRedSeg < kVuSegments,
              "VU zones must satisfy min < amber < red < segments");
static_assert(kVuPeakHoldMs > 0 && kVuPeakFallMs > 0, "VU peak timings must be positive");
static_assert(kClockPollMs > 0 && kWeatherPollMs > 0 && kWeatherStaleMs > 0, "poll / stale intervals must be positive");

}  // namespace main_layout

}  // namespace screens_cfg
