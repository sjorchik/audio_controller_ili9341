#pragma once

// Константи модуля ui/ (display, fonts, icons).
// Піни тут не дублюються: вони лише в config/pins.h (pins::kSt7789*; назви історичні, дисплей — ILI9341).
// Яскравість за замовчуванням — defaults::kDefaultBrightness (config/defaults.h).

#include <stddef.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// Прапорці умовної компіляції
// ---------------------------------------------------------------------------

// Тестовий кадр (статичний, без анімації, малюється один раз при старті):
// рамка + кольорові кути (перевірка зміщення вікна), рядки з українськими
// літерами трьома розмірами, алфавіт, усі IconId з підписами.
// Не заміна ui/screens. Вимкнути (0), коли зʼявляться екрани.
#define DISPLAY_DEMO 0

namespace display_cfg {

// ---------------------------------------------------------------------------
// Геометрія (ILI9341 320×240)
// ---------------------------------------------------------------------------
// [D1] Панель ILI9341: фізично 240×320 (портрет), у проєкті горизонтально 320×240.
// Зміщення вікна (offset) для ILI9341 дорівнює нулю й у конфігурації не потрібне.
constexpr int16_t kPanelW = 320;   // ширина панелі в горизонтальній орієнтації
constexpr int16_t kPanelH = 240;   // висота панелі в горизонтальній орієнтації

// Висота «старої» робочої області (екрани з ST7789 320×170). Усі існуючі екрани
// малюються лише в рядках 0..kLegacyViewportH-1; нижні (kPanelH - kLegacyViewportH)
// рядків лишаються чорними (до кроку 2: годинник / погода).
constexpr int16_t kLegacyViewportH = 170;

// Розміри, які бачить решта коду через DisplayManager::width()/height() і
// screens_cfg::kW/kH: це ОБЛАСТЬ ЕКРАНІВ (320×170), а не вся панель.
constexpr int16_t kWidth  = kPanelW;
constexpr int16_t kHeight = kLegacyViewportH;

static_assert(kLegacyViewportH > 0 && kLegacyViewportH <= kPanelH,
              "legacy viewport must fit into the panel");

// Поворот LovyanGFX, що дає ГОРИЗОНТАЛЬ 320×240 для ILI9341 (rotation 1).
// Якщо картинка повернута на 90° / дзеркальна — змінити саме цю константу
// (0..3; для горизонталі підходять лише непарні значення 1 і 3).
constexpr uint8_t kRotation = 1;

// Нормальна й розвернута на 180° орієнтації. Дисплей у корпусі може стояти в
// будь-який бік, тому орієнтація перемикається під час роботи:
// DisplayManager::setFlipped(bool) — налаштування в Settings/вебі. Розміри 320×240
// в обох випадках однакові.
constexpr uint8_t kRotationNormal  = kRotation;
constexpr uint8_t kRotationFlipped = static_cast<uint8_t>((kRotation + 2) & 3);

// Орієнтація при старті, поки Settings немає. true = kRotationFlipped.
// ПРИПУЩЕННЯ, ПЕРЕВІРИТИ НА ЗАЛІЗІ: значення true успадковано від ST7789-макетки
// (там з виводами модуля праворуч правильний напрямок давав саме Flipped). Для
// ILI9341 і нового корпусу напрямок може бути іншим: якщо картинка догори дригом —
// змінити на false (або перемкнути орієнтацію в налаштуваннях пристрою).
constexpr bool kDefaultFlipped = true;

// ПРИПУЩЕННЯ, ПЕРЕВІРИТИ: більшість модулів ILI9341 працюють БЕЗ інверсії.
// Ознака помилки: чорний фон виглядає білим, кольори «негатив». Тоді змінити на true.
constexpr bool kInvert = false;

// ПРИПУЩЕННЯ, ПЕРЕВІРИТИ: порядок RGB/BGR. Ознака помилки: червона смуга виглядає
// синьою (R↔B). Тоді змінити на true.
constexpr bool kRgbOrder = false;

// ---------------------------------------------------------------------------
// SPI
// ---------------------------------------------------------------------------
// Піни SCLK12/MOSI11/CS10 — нативні IOMUX-лінії FSPI. Для ILI9341 безпечна
// частота запису 40 МГц (деякі модулі стабільні й на 80 МГц, але за замовчуванням
// не ризикуємо: артефакти — «сніг», зсунуті рядки, збої кольору на довгих
// провідниках). Час передачі кадру області екранів = 320·170·16 біт / f:
// 40 МГц ≈ 22 мс (передається лише kLegacyViewportH рядків, не вся панель).
constexpr uint32_t kSpiWriteHz = 40000000;
constexpr uint8_t  kSpiMode    = 0;

// Передача кадру через DMA (кадр у PSRAM). За замовчуванням ВИМКНЕНО: поведінка
// DMA з PSRAM залежить від версії LovyanGFX/IDF і не перевірена на залізі.
// Увімкнути (true) після успішної перевірки: тоді передача кадру A йде у фоні,
// поки задача малює кадр B у другий буфер.
constexpr bool kUseDma = false;

// ---------------------------------------------------------------------------
// Підсвітка (LEDC PWM, лише ledcAttach())
// ---------------------------------------------------------------------------
// 20 кГц — вище за чутний діапазон (5 кГц давали б писк у аналоговому тракті
// поруч з аудіопроцесором). 10 біт: 80 МГц / (20 кГц · 1024) ≈ 3.9 — дільник
// LEDC у допустимих межах.
constexpr uint32_t kBacklightFreqHz         = 20000;
constexpr uint8_t  kBacklightResolutionBits = 10;
// true: високий рівень на BLK = підсвітка ввімкнена.
constexpr bool     kBacklightActiveHigh     = true;

// ---------------------------------------------------------------------------
// Задача малювання (FreeRTOS)
// ---------------------------------------------------------------------------
// Ядро 0 (UI), пріоритет 2 — за таблицею розділу 5 MASTER SPEC:
// введення = 4, AppController ≈ 3, UI = 2, мережа/веб ≈ 1. Число зі
// специфікації не змінювалось. Аудіотест (audio_cfg::kTestTaskPriority) = 1.
constexpr int      kTaskCore       = 0;
constexpr uint8_t  kTaskPriority   = 2;
constexpr uint32_t kTaskStackBytes = 8192;

// Період кадру, мс. 40 мс = 25 кадр/с: для VU-метра й UI достатньо плавно;
// при 40 МГц передача кадру ≈ 22 мс (без DMA, лише 170 рядків) + малювання ≈ 5..10 мс
// вкладаються в 40 мс, а 33 мс (30 кадр/с) лишили б замало запасу.
constexpr uint32_t kFramePeriodMs = 40;

// Кількість повнокадрових буферів (спрайтів) у PSRAM. [D1] Розмір кожного — вся панель
// 320×240: 2 × 320·240·2 = 307 200 Б (≈ 300 КіБ, ≈ 3.7 % від 8 МБ PSRAM).
constexpr uint8_t  kBufferCount = 2;

// ---------------------------------------------------------------------------
// Шрифти: власні GFXfont з DejaVu Sans (ui/font_data.h, генерує
// tools/gen_gfxfont.py). Значення — висота рядка (yAdvance) у пікселях; вони
// мусять збігатися з тими, що надрукує UiFonts::begin() у Serial.
// ---------------------------------------------------------------------------
constexpr uint8_t kFontLargePx = 24;   // FontSize::Large — рядок метаданих/статусу (P35), заголовок меню
constexpr uint8_t kFontSmallPx = 17;   // FontSize::Small — статус-рядок, тембр
constexpr uint8_t kFontTinyPx  = 14;   // FontSize::Tiny  — підписи, дрібні позначки
// [Prompt 32] FontSize::Digits — великі цифри спливного вікна (ui/font_digits_data.h,
// tools/gen_digits_font.py, em = 112). Лише символи ' ', '+', '-', '0'..'9'.
// Значення — yAdvance згенерованого шрифту (звіряє static_assert у ui/fonts.cpp).
constexpr uint8_t kFontDigitsPx = 131;
// [Prompt 35] FontSize::XLarge — назва станції на екрані Radio (ui/font_xlarge_data.h,
// tools/gen_xlarge_font.py, em = 30, DejaVu Sans, той самий набір символів, що й Large).
// Значення — yAdvance згенерованого шрифту (звіряє static_assert у ui/fonts.cpp).
constexpr uint8_t kFontXLargePx = 36;
// [S2c0] FontSize::Clock — час HH:MM основного екрана 320x240 (ui/font_clock_data.h,
// tools/gen_clock_font.py, em = 47, DejaVu Sans). Лише символи '0'..'9', ':' і '-'.
// Значення — yAdvance згенерованого шрифту (звіряє static_assert у ui/fonts.cpp).
// Висота цифри 34..36 px, відступ від верху рядка до верху цифри 9..10 px,
// нижче бази 12 px: малювати за верхом цифри, а не за верхом рядка шрифту.
constexpr uint8_t kFontClockPx = 56;

// ---------------------------------------------------------------------------
// Іконки: 1-бітна маска kIconSize × kIconSize, рядки по (kIconSize+7)/8 байт,
// старший біт зліва; колір задає викликач.
// ---------------------------------------------------------------------------
constexpr uint8_t kIconSize = 24;

// ---------------------------------------------------------------------------
// Кольори (RGB565, тип uint16_t — LovyanGFX трактує його як RGB565)
// ---------------------------------------------------------------------------
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

constexpr uint16_t kColorBlack  = rgb565(0, 0, 0);
constexpr uint16_t kColorWhite  = rgb565(255, 255, 255);
constexpr uint16_t kColorRed    = rgb565(255, 0, 0);
constexpr uint16_t kColorGreen  = rgb565(0, 255, 0);
constexpr uint16_t kColorBlue   = rgb565(0, 0, 255);
constexpr uint16_t kColorYellow = rgb565(255, 220, 0);
constexpr uint16_t kColorGray   = rgb565(140, 140, 140);

// Кольори за замовчуванням для примітивів.
constexpr uint16_t kColorBg = kColorBlack;
constexpr uint16_t kColorFg = kColorWhite;

// ---------------------------------------------------------------------------
// Розкладка тестового кадру (DISPLAY_DEMO). Лише для перевірки заліза.
// Малюється на ВСІЙ панелі 320×240 (kPanelW × kPanelH).
// ---------------------------------------------------------------------------
namespace demo {
constexpr int16_t kFrameThickness = 2;    // товщина рамки по краю всієї панелі
constexpr int16_t kCornerSize     = 10;   // кольорові квадрати в кутах
constexpr int16_t kCornerInset    = 4;    // відступ квадратів від краю (всередину рамки)
constexpr int16_t kBarsX          = 5;    // лівий край смуг
constexpr int16_t kBarsY          = 20;   // верх смуг
constexpr int16_t kBarW           = 62;   // ширина смуги (5 × 62 = 310 → до x = 315)
constexpr int16_t kBarH           = 56;   // висота смуг
constexpr int16_t kBarCount       = 5;    // R, G, B, білий, чорний
constexpr int16_t kLabelY         = 80;   // Tiny: підписи під смугами
constexpr int16_t kTextX          = 16;   // лівий відступ тексту
constexpr int16_t kTitleY         = 102;  // Large: «Ґрунт Їжачок Єдність» (перевірка шрифту)
constexpr int16_t kInfoY          = 134;  // Tiny: параметри панелі
constexpr int16_t kHintY          = 150;  // Tiny: пояснення лінії межі
constexpr int16_t kBelowY         = 180;  // Tiny: текст у нижній (70 рядків) області
}  // namespace demo

}  // namespace display_cfg
