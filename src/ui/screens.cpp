#include "ui/screens.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include <type_traits>

#include "config/defaults.h"
#include "config/display_config.h"
#include "config/eq_config.h"          // [Prompt 49a] eq_cfg::kBandCount / kBandFreqHz (тимчасовий екран EQ)
#include "config/eq_screen_config.h"   // [Prompt 49a/49b] розкладка екрана EQ + Balance
#include "config/screens_config.h"
#include "audio/vu_source.h"      // [Prompt 17] рівні VU (читаються напряму); [Prompt 48d] + VuSourceLinePath
#include "config/features.h"
#include "config/line_path_config.h"  // [Prompt 48d] LINE_PATH_ENABLE (раніше приходив через adc_vu.h)
#include "core/action_names.h"  // [Prompt 15] ДОДАНО: імена дій (чисті дані)
#include "core/app_state.h"
#include "ui/display.h"
#include "ui/fonts.h"
#include "stations/station_store.h"  // [Prompt 11] ДОДАНО: чисті дані, не залізо
#include "ui/icons.h"
#include <math.h>
#include "config/ua_text_config.h"   // [S2c1] українські тексти основного екрана
#include "core/time_service.h"       // [S2c1] лише читання (getLocal)
#include "core/weather_codes.h"      // [S2c1]
#include "core/weather_service.h"    // [S2c1] лише читання (getStatus)

#if DISPLAY_DEMO
#warning "DISPLAY_DEMO = 1: постав 0 у config/display_config.h, щоб показувати реальні екрани"
#endif

// [Prompt 11] ВИНЯТОК З ІЗОЛЯЦІЇ: окрім AppState::snapshot(), цей файл читає
// UiFonts/UiIcons і StationStore::count()/get() (список станцій потрібен одразу
// для кількох сусідніх рядків). StationStore — модуль чистих даних, не залізо,
// тож правило «AppController — єдиний власник заліза» не порушується.
// [Prompt 15] Так само читається core/action_names (таблиця імен без стану); стан навчання
// IR береться з AppState (irLearnStatus/irLearnTarget/irLearnConflictWith), не з IrRc5.
// [Prompt 16] Екран Mode::OtaUpdate читає лише AppState.otaProgress.
// [Prompt 23b] Назва входу (лише ExternalInput; з Radio її прибрано в P33) береться з
// AppState.inputName; Settings екрани не читають. Довга назва обрізається fitText() з "..."
// по межі UTF-8-символу.
// [Prompt 28] AppState.offline: замість Wi-Fi-індикатора у верхній панелі — текст "offline" (Tiny);
// на екрані WifiSetup — підказка "OK: work offline"; AppState.restarting — екран "Restarting...".
// [Prompt 32] Спливне вікно параметра: UI веде ЛИШЕ відлік показу (millis() від моменту, коли
// AppState.popupSeq змінився); сам параметр і його значення читаються зі знімка AppState.
// Статус-рядок з ціллю енкодера (drawStatusRow) прибрано.
// [Prompt 33] Екран Radio: назву входу прибрано; рядок метаданих і рядок статусу потоку обʼєднано
// в один (слот kTrackY, marquee s_track): метадані, коли Playing і вони непорожні, інакше статус.
// [Prompt 34] VU перемальовано під індикатор «Маяк-233» (сегментні смуги L/R, шкала в дБ, утримання
// піку в UI); балістику VuSource не змінено. Три крапки в статусах — ASCII "...", не «…».
// [Prompt 35] Екран Radio: назва станції — FontSize::XLarge (під рядком іконок), обʼєднаний рядок
// метадані / статус — FontSize::Large; метадані малюються kColorTrack (світліше за kColorDim).
// [Prompt 36] Екран Radio: у смузі іконок ліворуч — SSID і IP (з AppState, drawNetInfo()), іконка Wi-Fi
// за AppState.wifiRssi (Wifi1..3) з гістерезисом у UI (wifiBarsUpdate()); WifiManager екран не чіпає.
// [Prompt 38] Екран ExternalInput: напис "INPUT" прибрано; назва входу — FontSize::XLarge, по центру
// вільної області між смугою іконок і VU (kExtNameY), колір за номером входу (kColorInputName).
// [Prompt 49a] ТИМЧАСОВИЙ текстовий екран «EQ + Баланс» (Mode::Menu + MenuContext::EqBalance): заголовок, шість
// рядків (5 смуг EQ за kBandFreqHz і «Bal») зі значенням «%+d», маркер ">" і підсвітка вибраного. Лише для
// перевірки логіки P49a; справжні вертикальні стовпчики малює P49b. Читає лише AppState (eqSelected, eqGainsDb, balanceDb).
// [Prompt 49b] Екран «EQ + Balance» — вертикальні стовпчики (смуги EQ + баланс), розкладка в
// config/eq_screen_config.h; читає лише AppState (eqSelected, eqGainsDb, balanceDb, inputName).
// [E4] Екран «EQ + Balance»: 10 смуг (eq_cfg::kBandCount, набір частот EQ_BAND_SET) + баланс = 11 колонок по 27 px. Підписи
// частот формуються з eq_cfg::kBandFreqHz (formatBandLabel); значення підбирають кегль і знак за шириною колонки
// (fitBandValue). Читає лише AppState, як і раніше.
// [Prompt 17] ВИНЯТОК З ІЗОЛЯЦІЇ: рівні VU беруться напряму з VuSourceDecodedPcm::read() (з
// частотою кадру), МИНАЮЧИ AppState; AppStateData.vuLeft/vuRight не використовуються.

// [S2c1] Основний екран (Mode::Radio / Mode::ExternalInput, включно з Computer / Aux / Player) малюється на
// ВСЮ панель 320×240 (screens_cfg::main_layout): смуга стану, годинник і дата (TimeService::getLocal),
// погода (WeatherService::getStatus, раз на секунду, без мережі), тег джерела, назва станції / входу,
// рядок треку, VU L / R і число гучності. Старі drawRadio / drawExternal / верхня смуга іконок / «Маяк-233»
// прибрано. Решта екранів лишається у верхніх 320×170 (DisplayManager::setFullPanel(false)).
// Часткова передача: якщо змінились лише VU і / або рядки marquee, на панель іде лише смуга з цими
// рядками (DisplayManager::setPushBand), інакше весь кадр. Мова написів основного екрана — українська
// (рішення власника 2026-11-27); «Restarting...», EQ, список станцій тощо — як раніше.

namespace {

using D = DisplayManager;
namespace c = screens_cfg;
namespace dc = display_cfg;
namespace ml = screens_cfg::main_layout;

static_assert(std::is_trivially_copyable<AppStateData>::value,
              "AppStateData має бути POD для memcmp");

// ---------------------------------------------------------------------------
// Стан UI (лише візуальний, не частина AppStateData)
// ---------------------------------------------------------------------------
struct Marquee {
    char     text[128];
    int32_t  textW;
    int32_t  offset;
    uint32_t lastStepMs;
    uint32_t holdUntilMs;
    bool     atEnd;
    bool     seeded;
};

Marquee s_station = {};
Marquee s_track   = {};   // [Prompt 33] обʼєднаний рядок «метадані / статус»

// [Prompt 33] Остання побачена станція: зміна індексу скидає s_track на початок, навіть якщо
// текст рядка (напр. статус) лишився тим самим.
bool     s_trackStationKnown = false;
uint16_t s_trackStation      = 0;

// [Prompt 17] VU: джерело (читає лише display-задача) і кількість засвічених сегментів
// минулого кадру (перемальовуємо, лише коли вона змінилась).
// [Prompt 34] + утримання піку по каналах (лише UI) і попередньо обчислені мітки шкали.
VuSourceDecodedPcm s_vu;
VuSourceLinePath s_vuLine;   // [Prompt 48d] VU зовнішніх входів (будь-який логічний вхід >= 1) зі line_path
int s_vuLitL = 0;
int s_vuLitR = 0;
int s_vuPeakDrawnL = 0;   // [Prompt 34] пік, намальований минулого кадру
int s_vuPeakDrawnR = 0;

struct VuPeak {
    int      seg;           // кількість сегментів піку (0 = немає; пік = сегмент з індексом seg-1)
    uint32_t holdUntilMs;   // до цього моменту пік утримується
    uint32_t lastFallMs;    // момент останнього кроку спаду
};
VuPeak s_peakL = {};
VuPeak s_peakR = {};

// [S2c1] Знімки даних основного екрана (лише UI). Порівнюються memcmp, тож структури обнуляються (memset)
// перед заповненням; зміна знімка = потрібен повний вивід кадру.
struct ClockView {
    bool valid;
    char time[8];    // "HH:MM" або "--:--"
    char date[40];   // "пт, 27 листопада" або «Час невідомий»
};
struct WeatherView {
    bool    set;     // місто задано
    bool    valid;   // є дані для цього міста
    bool    stale;   // дані старші за ml::kWeatherStaleMs
    bool    isDay;
    int16_t temp;    // °C, округлено
    int16_t windDm;  // вітер, дм/с (м/с * 10)
    uint8_t humidity;   // %
    uint16_t pressureMm;    // [S2b3] мм рт. ст. (з гПа сервісу); 0 = немає даних
    int16_t  windDirDeg;    // напрямок «звідки», 0..359; -1 = немає даних
    uint8_t cond;    // weather::Condition
    char    city[weather_cfg::kNameBytes];
};
ClockView   s_clock = {};
WeatherView s_wx    = {};
bool     s_clockPolled   = false;
bool     s_weatherPolled = false;
uint32_t s_clockNextMs   = 0;
uint32_t s_weatherNextMs = 0;
int      s_wifiBarsDrawn = -1;   // s_wifiBars, врахований у останньому кадрі

// [Prompt 36] Кількість «рисок» іконки Wi-Fi (0 = не підключено), стан гістерезису лише в UI.
uint8_t s_wifiBars = 0;
constexpr uint8_t kWifiBarsFull = 3;

// [Prompt 32] Спливне вікно: останній побачений popupSeq і час закінчення показу.
bool     s_popupSeqKnown = false;
uint8_t  s_popupSeqSeen  = 0;
bool     s_popupActive   = false;
uint32_t s_popupUntilMs  = 0;

AppStateData s_prev;            // попередній знімок
bool         s_havePrev = false;
bool         s_modeKnown = false;
Mode         s_prevMode = Mode::Standby;
uint8_t      s_savedBrightness = defaults::kDefaultBrightness;

// ---------------------------------------------------------------------------
// Marquee
// ---------------------------------------------------------------------------
// Повертає true, якщо вигляд змінився (новий текст або зсув) і потрібен кадр.
bool marqueeUpdate(Marquee& m, const char* src, FontSize size, uint32_t now) {
    if (!m.seeded || strcmp(m.text, src) != 0) {
        strlcpy(m.text, src, sizeof(m.text));
        m.textW       = m.text[0] ? UiFonts::textWidth(m.text, size) : 0;
        m.offset      = 0;
        m.lastStepMs  = now;
        m.holdUntilMs = now + c::kMarqueeEdgePauseMs;
        m.atEnd       = false;
        m.seeded      = true;
        return true;
    }
    if (m.textW <= c::kContentW) return false;

    if (static_cast<int32_t>(now - m.holdUntilMs) < 0) {   // пауза
        m.lastStepMs = now;
        return false;
    }
    if (m.atEnd) {                                          // кінець -> початок
        m.offset      = 0;
        m.atEnd       = false;
        m.holdUntilMs = now + c::kMarqueeEdgePauseMs;
        m.lastStepMs  = now;
        return true;
    }
    const uint32_t steps = (now - m.lastStepMs) / c::kMarqueeStepMs;
    if (steps == 0) return false;
    m.lastStepMs += steps * c::kMarqueeStepMs;
    m.offset += static_cast<int32_t>(steps) * c::kMarqueeStepPx;
    const int32_t maxOff = m.textW - c::kContentW;
    if (m.offset >= maxOff) {
        m.offset      = maxOff;
        m.atEnd       = true;
        m.holdUntilMs = now + c::kMarqueeEdgePauseMs;
    }
    return true;
}

// DisplayManager не має clip-прямокутника: текст малюється зі зсувом, а потім
// бічні поля перекриваються кольором фону. Тому рядок marquee має займати
// всю ширину екрана без іншого вмісту.
void marqueeDraw(const Marquee& m, int16_t y, FontSize size, uint16_t color) {
    if (!m.text[0]) return;
    const bool scrolling = m.textW > c::kContentW;
    const int16_t x = static_cast<int16_t>(c::kMargin - (scrolling ? m.offset : 0));
    D::drawText(m.text, x, y, size, color);
    if (scrolling) {
        const int16_t h = static_cast<int16_t>(UiFonts::lineHeight(size));
        D::fillRect(0, y, c::kMargin, h, dc::kColorBg);
        D::fillRect(c::kW - c::kMargin, y, c::kMargin, h, dc::kColorBg);
    }
}

// ---------------------------------------------------------------------------
// Допоміжні малювання
// ---------------------------------------------------------------------------
void drawCentered(const char* text, int16_t y, FontSize size, uint16_t color) {
    int32_t x = (c::kW - UiFonts::textWidth(text, size)) / 2;
    if (x < 0) x = 0;
    D::drawText(text, static_cast<int16_t>(x), y, size, color);
}

// Вписує src у maxW пікселів: за потреби обрізає по межі UTF-8-символу й додає
// "...". out має вміщати кінцевий текст: cap >= sizeof(Station::name) + 8.
void fitText(const char* src, char* out, size_t cap, FontSize size, int32_t maxW) {
    strlcpy(out, src, cap);
    if (UiFonts::textWidth(out, size) <= maxW) return;
    size_t len = strlen(src);
    while (len > 0) {
        --len;
        while (len > 0 && (static_cast<uint8_t>(src[len]) & 0xC0) == 0x80) --len;
        memcpy(out, src, len);
        strcpy(out + len, "...");
        if (UiFonts::textWidth(out, size) <= maxW) return;
    }
}

// [Prompt 23b] Назва входу береться з AppState.inputName (її вирішує AppController: користувацька
// з налаштувань, запасно defaults::kInputNames). Тут лише останній запасний варіант на випадок
// порожнього поля (нульовий AppState до першої публікації): типова назва за індексом або "Input".
// [Prompt 33] Використовується лише екраном ExternalInput.
const char* inputName(const AppStateData& s) {
    if (s.inputName[0] != '\0') return s.inputName;
    return (s.inputIndex < defaults::kInputCount) ? defaults::kInputNames[s.inputIndex] : "Input";
}

// Буфер під fitText для назви входу: cap >= sizeof(inputName) + 8.
constexpr size_t kInputFitCap = kInputNameMax + 8;

// [Prompt 36] Оновлює s_wifiBars за знімком. Без звʼязку — 0. Підключено, але RSSI невідомий —
// лишається попереднє значення (початково повна іконка, як було до P36). Далі гістерезис.
int wifiBarThreshold(uint8_t bars) {
    return bars >= 3 ? c::kWifiRssiBar3Db : c::kWifiRssiBar2Db;   // для bars = 3 і 2
}

void wifiBarsUpdate(const AppStateData& s) {
    if (!s.wifiConnected) {
        s_wifiBars = 0;
        return;
    }
    if (s.wifiRssi >= 0) {   // RSSI ще невідомий
        if (s_wifiBars == 0) s_wifiBars = kWifiBarsFull;
        return;
    }
    const int r = s.wifiRssi;
    if (s_wifiBars == 0) {   // перше значення: без гістерезису
        s_wifiBars = (r >= c::kWifiRssiBar3Db) ? 3 : (r >= c::kWifiRssiBar2Db) ? 2 : 1;
        return;
    }
    const int half = c::kWifiRssiHystDb / 2;
    while (s_wifiBars < 3 && r >= wifiBarThreshold(static_cast<uint8_t>(s_wifiBars + 1)) + half) {
        ++s_wifiBars;
    }
    while (s_wifiBars > 1 && r < wifiBarThreshold(s_wifiBars) - half) {
        --s_wifiBars;
    }
}

// [S2c1] Чи настав час чергового опитування (самосинхронізація: після довгої перерви — одразу).
bool pollDue(bool& polled, uint32_t& nextMs, uint32_t periodMs, uint32_t now) {
    if (polled && static_cast<int32_t>(now - nextMs) < 0) return false;
    polled = true;
    nextMs = now + periodMs;
    return true;
}

// [S2c1] Оновлює s_clock з TimeService (неблокуюче). true — знімок змінився (потрібен кадр).
bool clockUpdate(uint32_t now) {
    if (!pollDue(s_clockPolled, s_clockNextMs, ml::kClockPollMs, now)) return false;
    ClockView v;
    memset(&v, 0, sizeof(v));
    tm lt;
    if (TimeService::getLocal(lt)) {
        v.valid = true;
        snprintf(v.time, sizeof(v.time), "%02u:%02u", static_cast<unsigned>(lt.tm_hour % 24),
                 static_cast<unsigned>(lt.tm_min % 60));
        const int wd = (lt.tm_wday >= 0 && lt.tm_wday < 7) ? lt.tm_wday : 0;
        const int mo = (lt.tm_mon >= 0 && lt.tm_mon < 12) ? lt.tm_mon : 0;
        snprintf(v.date, sizeof(v.date), "%s, %d %s", ua_text::kDayShort[wd], lt.tm_mday,
                 ua_text::kMonthGen[mo]);
    } else {
        strlcpy(v.time, ua_text::kClockUnknown, sizeof(v.time));
        strlcpy(v.date, ua_text::kTimeUnknown, sizeof(v.date));
    }
    if (memcmp(&v, &s_clock, sizeof(v)) == 0) return false;
    s_clock = v;
    return true;
}

// [S2c1] Оновлює s_wx зі знімка WeatherService (м'ютекс на мікросекунди, без мережі). true — змінилось.
bool weatherUpdate(uint32_t now) {
    if (!pollDue(s_weatherPolled, s_weatherNextMs, ml::kWeatherPollMs, now)) return false;
    WeatherService::Status st;
    if (!WeatherService::getStatus(st)) return false;   // м'ютекс зайнятий: лишаємо попередній знімок
    WeatherView v;
    memset(&v, 0, sizeof(v));
    v.set = st.loc.set;
    strlcpy(v.city, st.loc.name, sizeof(v.city));
    v.valid = st.valid;
    if (st.valid) {
        long t = lroundf(st.cur.tempC);
        if (t < -99) t = -99;
        if (t > 99) t = 99;
        v.temp  = static_cast<int16_t>(t);
        long wd = lroundf(st.cur.windMs * 10.0f);
        if (wd < 0) wd = 0;
        if (wd > 999) wd = 999;
        v.windDm    = static_cast<int16_t>(wd);
        v.humidity  = st.cur.humidity > 100 ? 100 : st.cur.humidity;
        // [S2b3] Тиск: сервіс віддає гПа (0 = немає даних), екран показує мм рт. ст.
        v.pressureMm  = (st.cur.pressureHpa > 0)
                            ? static_cast<uint16_t>(lroundf(st.cur.pressureHpa * weather_cfg::kHpaToMmHg))
                            : 0;
        v.windDirDeg  = st.cur.windDirDeg;
        v.isDay = st.cur.isDay;
        v.cond  = static_cast<uint8_t>(weather::category(st.cur.code, st.cur.isDay));
        v.stale = (st.ageMs != WeatherService::kNoAge) && (st.ageMs > ml::kWeatherStaleMs);
    }
    if (memcmp(&v, &s_wx, sizeof(v)) == 0) return false;
    s_wx = v;
    return true;
}

// [S2c1] VU: два рядки L / R (сегменти зелений -> бурштиновий -> червоний), число гучності праворуч.
// Сегмент i світиться, коли i < lit; пік — окремий яскравий сегмент peak-1. Кожен сегмент — окремий
// fillRect (між ними проміжок кольору тла). ENABLE_VU == 0: порожня рамка.
#if ENABLE_VU
int vuToSegments(float level) {
    int n = static_cast<int>(level * ml::kVuSegments + 0.5f);
    if (n < 0) n = 0;
    if (n > ml::kVuSegments) n = ml::kVuSegments;
    return n;
}

// Один крок утримання піку: нове значення >= піку — пік піднімається й утримується; після
// kVuPeakHoldMs спадає на сегмент за kVuPeakFallMs, але не нижче поточного рівня.
void vuPeakUpdate(VuPeak& p, int lit, uint32_t now) {
    if (lit >= p.seg) {
        p.seg         = lit;
        p.holdUntilMs = now + ml::kVuPeakHoldMs;
        p.lastFallMs  = p.holdUntilMs;
        return;
    }
    if (static_cast<int32_t>(now - p.holdUntilMs) < 0) return;   // ще утримується
    const uint32_t steps = (now - p.lastFallMs) / ml::kVuPeakFallMs;
    if (steps == 0) return;
    p.lastFallMs += steps * ml::kVuPeakFallMs;
    p.seg = (steps >= static_cast<uint32_t>(p.seg - lit)) ? lit : p.seg - static_cast<int>(steps);
}

void vuPeakReset(VuPeak& p) {
    p.seg         = 0;
    p.holdUntilMs = 0;
    p.lastFallMs  = 0;
}

void drawVuBar(int16_t y, int lit, int peak) {
    const int peakIdx = peak - 1;   // -1, якщо піку немає
    for (uint8_t i = 0; i < ml::kVuSegments; ++i) {
        const bool on = (i < lit) || (static_cast<int>(i) == peakIdx);
        uint16_t color;
        if (i >= ml::kVuRedSeg)        color = on ? ml::kColorVuRed   : ml::kColorVuRedOff;
        else if (i >= ml::kVuAmberSeg) color = on ? ml::kColorVuAmber : ml::kColorVuAmberOff;
        else                           color = on ? ml::kColorVuGreen : ml::kColorVuGreenOff;
        const int16_t x = static_cast<int16_t>(ml::kVuBarX + i * ml::kVuSegPitch);
        D::fillRect(x, y, ml::kVuSegW, ml::kVuBarH, color);
    }
}
#endif

void drawVu(int litL, int litR, int peakL, int peakR) {
#if ENABLE_VU
    drawVuBar(ml::kVuLY, litL, peakL);
    drawVuBar(ml::kVuRY, litR, peakR);
    const int16_t dy = static_cast<int16_t>((ml::kVuBarH - dc::kFontTinyPx) / 2);
    D::drawText("L", ml::kVuLabelX, static_cast<int16_t>(ml::kVuLY + dy), FontSize::Tiny, ml::kColorVuLabel);
    D::drawText("R", ml::kVuLabelX, static_cast<int16_t>(ml::kVuRY + dy), FontSize::Tiny, ml::kColorVuLabel);
#else
    (void)litL;
    (void)litR;
    (void)peakL;
    (void)peakR;
    D::drawRect(ml::kVuBarX, ml::kVuLY, ml::kVuUsedW, ml::kVuBandH, c::kColorDim);
#endif
}

// [S2c1] Заповнений круг рядками (DisplayManager не має кіл): 2r+1 вузьких fillRect; r <= ~12.
void fillCircle(int16_t cx, int16_t cy, int16_t r, uint16_t color) {
    for (int16_t dy = -r; dy <= r; ++dy) {
        int16_t dx = 0;
        while ((dx + 1) * (dx + 1) + dy * dy <= r * r) ++dx;
        D::fillRect(static_cast<int16_t>(cx - dx), static_cast<int16_t>(cy + dy),
                    static_cast<int16_t>(2 * dx + 1), 1, color);
    }
}

// Лінія завтовшки 2 px (дві сусідні вертикалі).
void drawLine2(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    D::drawLine(x0, y0, x1, y1, color);
    D::drawLine(static_cast<int16_t>(x0 + 1), y0, static_cast<int16_t>(x1 + 1), y1, color);
}

// Хмарка в рамці 32×20 (x, y — лівий верх): три круги й пласка основа.
void drawCloud(int16_t x, int16_t y, uint16_t color) {
    fillCircle(static_cast<int16_t>(x + 9), static_cast<int16_t>(y + 12), 7, color);
    fillCircle(static_cast<int16_t>(x + 17), static_cast<int16_t>(y + 8), 8, color);
    fillCircle(static_cast<int16_t>(x + 25), static_cast<int16_t>(y + 12), 6, color);
    D::fillRect(static_cast<int16_t>(x + 9), static_cast<int16_t>(y + 12), 17, 8, color);
}

// Сонце: круг радіуса r і (за потреби) 8 променів.
void drawSun(int16_t cx, int16_t cy, int16_t r, bool rays, uint16_t color) {
    fillCircle(cx, cy, r, color);
    if (!rays) return;
    const int16_t in = static_cast<int16_t>(r + 4);
    const int16_t out = static_cast<int16_t>(r + 8);
    const int16_t din = static_cast<int16_t>(in * 7 / 10);
    const int16_t dout = static_cast<int16_t>(out * 7 / 10);
    D::drawLine(static_cast<int16_t>(cx + in), cy, static_cast<int16_t>(cx + out), cy, color);
    D::drawLine(static_cast<int16_t>(cx - in), cy, static_cast<int16_t>(cx - out), cy, color);
    D::drawLine(cx, static_cast<int16_t>(cy + in), cx, static_cast<int16_t>(cy + out), color);
    D::drawLine(cx, static_cast<int16_t>(cy - in), cx, static_cast<int16_t>(cy - out), color);
    D::drawLine(static_cast<int16_t>(cx + din), static_cast<int16_t>(cy + din),
                static_cast<int16_t>(cx + dout), static_cast<int16_t>(cy + dout), color);
    D::drawLine(static_cast<int16_t>(cx - din), static_cast<int16_t>(cy + din),
                static_cast<int16_t>(cx - dout), static_cast<int16_t>(cy + dout), color);
    D::drawLine(static_cast<int16_t>(cx + din), static_cast<int16_t>(cy - din),
                static_cast<int16_t>(cx + dout), static_cast<int16_t>(cy - dout), color);
    D::drawLine(static_cast<int16_t>(cx - din), static_cast<int16_t>(cy - din),
                static_cast<int16_t>(cx - dout), static_cast<int16_t>(cy - dout), color);
}

// Місяць: круг і зміщений круг кольору тла (серп відкритий угору-вправо).
void drawMoon(int16_t cx, int16_t cy, int16_t r, uint16_t color) {
    fillCircle(cx, cy, r, color);
    fillCircle(static_cast<int16_t>(cx + r / 2 + 1), static_cast<int16_t>(cy - r / 3), static_cast<int16_t>(r - 1),
               ml::kColorBg);
}

// [S2c1] Іконка погоди 36×36 з примітивів (x, y — лівий верх). stale — усе приглушеним кольором.
// isDay впливає на Clear / PartlyCloudy (сонце / місяць).
void drawWeatherIcon(weather::Condition cond, bool isDay, bool stale, int16_t x, int16_t y) {
    const uint16_t sun   = stale ? ml::kColorWxStale : ml::kColorSun;
    const uint16_t moon  = stale ? ml::kColorWxStale : ml::kColorMoon;
    const uint16_t cl    = stale ? ml::kColorWxStale : ml::kColorCloudLight;
    const uint16_t cd    = stale ? ml::kColorWxStale : ml::kColorCloudDark;
    const uint16_t rain  = stale ? ml::kColorWxStale : ml::kColorRain;
    const uint16_t snow  = stale ? ml::kColorWxStale : ml::kColorSnow;
    const uint16_t bolt  = stale ? ml::kColorWxStale : ml::kColorBolt;
    const uint16_t fog   = stale ? ml::kColorWxStale : ml::kColorFog;
    switch (cond) {
        case weather::Condition::Clear:
            if (isDay) drawSun(static_cast<int16_t>(x + 18), static_cast<int16_t>(y + 18), 8, true, sun);
            else       drawMoon(static_cast<int16_t>(x + 18), static_cast<int16_t>(y + 18), 11, moon);
            break;
        case weather::Condition::PartlyCloudy:
            if (isDay) drawSun(static_cast<int16_t>(x + 13), static_cast<int16_t>(y + 12), 6, true, sun);
            else       drawMoon(static_cast<int16_t>(x + 13), static_cast<int16_t>(y + 12), 8, moon);
            drawCloud(static_cast<int16_t>(x + 4), static_cast<int16_t>(y + 14), cl);
            break;
        case weather::Condition::Cloudy:
            drawCloud(static_cast<int16_t>(x + 4), static_cast<int16_t>(y + 4), cd);
            drawCloud(static_cast<int16_t>(x + 1), static_cast<int16_t>(y + 14), cl);
            break;
        case weather::Condition::Fog:
            drawCloud(static_cast<int16_t>(x + 2), static_cast<int16_t>(y + 2), cd);
            for (int16_t i = 0; i < 3; ++i) {
                D::fillRect(static_cast<int16_t>(x + 3), static_cast<int16_t>(y + 25 + i * 4), 30, 2, fog);
            }
            break;
        case weather::Condition::Drizzle:
            drawCloud(static_cast<int16_t>(x + 2), static_cast<int16_t>(y + 4), cl);
            for (int16_t i = 0; i < 4; ++i) {
                D::fillRect(static_cast<int16_t>(x + 7 + i * 7), static_cast<int16_t>(y + 27 + ((i & 1) ? 4 : 0)),
                            2, 3, rain);
            }
            break;
        case weather::Condition::Rain:
            drawCloud(static_cast<int16_t>(x + 2), static_cast<int16_t>(y + 2), cl);
            for (int16_t i = 0; i < 4; ++i) {
                const int16_t x0 = static_cast<int16_t>(x + 9 + i * 7);
                drawLine2(static_cast<int16_t>(x0 + 3), static_cast<int16_t>(y + 25), x0, static_cast<int16_t>(y + 34), rain);
            }
            break;
        case weather::Condition::Snow:
            drawCloud(static_cast<int16_t>(x + 2), static_cast<int16_t>(y + 2), cl);
            for (int16_t i = 0; i < 3; ++i) {
                const int16_t fx = static_cast<int16_t>(x + 9 + i * 9);
                const int16_t fy = static_cast<int16_t>(y + 30);
                D::drawLine(static_cast<int16_t>(fx - 3), fy, static_cast<int16_t>(fx + 3), fy, snow);
                D::drawLine(fx, static_cast<int16_t>(fy - 3), fx, static_cast<int16_t>(fy + 3), snow);
                D::drawLine(static_cast<int16_t>(fx - 2), static_cast<int16_t>(fy - 2),
                            static_cast<int16_t>(fx + 2), static_cast<int16_t>(fy + 2), snow);
                D::drawLine(static_cast<int16_t>(fx - 2), static_cast<int16_t>(fy + 2),
                            static_cast<int16_t>(fx + 2), static_cast<int16_t>(fy - 2), snow);
            }
            break;
        case weather::Condition::Thunder:
            drawCloud(static_cast<int16_t>(x + 2), static_cast<int16_t>(y + 2), cd);
            drawLine2(static_cast<int16_t>(x + 20), static_cast<int16_t>(y + 21), static_cast<int16_t>(x + 14),
                      static_cast<int16_t>(y + 29), bolt);
            drawLine2(static_cast<int16_t>(x + 14), static_cast<int16_t>(y + 29), static_cast<int16_t>(x + 19),
                      static_cast<int16_t>(y + 29), bolt);
            drawLine2(static_cast<int16_t>(x + 19), static_cast<int16_t>(y + 29), static_cast<int16_t>(x + 15),
                      static_cast<int16_t>(y + 35), bolt);
            break;
        case weather::Condition::Unknown:
        default: {
            const int32_t w = UiFonts::textWidth("?", FontSize::Large);
            D::drawText("?", static_cast<int16_t>(x + (ml::kWeatherIconSize - w) / 2),
                        static_cast<int16_t>(y + (ml::kWeatherIconSize - dc::kFontLargePx) / 2), FontSize::Large,
                        ml::kColorWxState);
            break;
        }
    }
}

// [S2c1] Стрілка напрямку вітру з ліній (дрібна). deg — метеорологічний напрямок «ЗВІДКИ» (0 = північний,
// за годинниковою); стрілка показує, КУДИ дме (deg + 180). deg < 0 (даних немає) — маленька крапка.
void drawWindArrow(int16_t cx, int16_t cy, int16_t r, int16_t deg, uint16_t color) {
    if (deg < 0) {
        fillCircle(cx, cy, 1, color);
        return;
    }
    const float a  = (static_cast<float>(deg) + 180.0f) * 0.01745329f;
    const float dx = sinf(a);     // одиничний вектор «куди» в координатах екрана (y вниз)
    const float dy = -cosf(a);
    const auto px = [&](float k) { return static_cast<int16_t>(cx + lroundf(dx * k)); };
    const auto py = [&](float k) { return static_cast<int16_t>(cy + lroundf(dy * k)); };
    const int16_t tx = px(r);
    const int16_t ty = py(r);
    D::drawLine(px(-r), py(-r), tx, ty, color);
    // Два «крила» наконечника: назад від вістря, повернуті на ±35°.
    const float cs = 0.819f;
    const float sn = 0.574f;
    const float bx = -dx;
    const float by = -dy;
    const float wl = static_cast<float>(r) * 0.7f;
    D::drawLine(tx, ty, static_cast<int16_t>(tx + lroundf((bx * cs - by * sn) * wl)),
                static_cast<int16_t>(ty + lroundf((bx * sn + by * cs) * wl)), color);
    D::drawLine(tx, ty, static_cast<int16_t>(tx + lroundf((bx * cs + by * sn) * wl)),
                static_cast<int16_t>(ty + lroundf((-bx * sn + by * cs) * wl)), color);
}

// [S2c1] Смуга стану: іконка Wi-Fi за RSSI (s_wifiBars) і SSID ліворуч, IP праворуч. Офлайн — «offline»;
// немає звʼязку — червона WifiOff і «Connecting...»; AP на основному екрані без тексту (екран AP — WifiSetup).
// Іконка мʼюту тут НЕ малюється (вона в зоні гучності).
constexpr size_t kStatusFitCap = sizeof(AppStateData::wifiSsid) + 8;   // запас під "..." для fitText

void drawStatusBar(const AppStateData& s) {
    D::fillRect(0, 0, ml::kPanelW, ml::kStatusH, ml::kColorStatusBg);
    if (s.offline) {
        D::drawText("offline", static_cast<int16_t>(ml::kStatusIconX + 2), ml::kStatusTextY, FontSize::Tiny,
                    c::kColorAccent);
        return;
    }
    const IconId icon = s.wifiConnected
                            ? UiIcons::wifiForBars(s_wifiBars ? s_wifiBars : kWifiBarsFull)
                            : IconId::WifiOff;
    D::drawIcon(icon, ml::kStatusIconX, ml::kStatusIconY, s.wifiConnected ? dc::kColorFg : c::kColorBad);
    if (s.wifiApMode) return;
    if (!s.wifiConnected) {
        D::drawText("Connecting...", ml::kStatusTextX, ml::kStatusTextY, FontSize::Tiny, ml::kColorStatusText);
        return;
    }
    int32_t ipW = 0;
    if (s.wifiIp[0] != '\0') {
        ipW = UiFonts::textWidth(s.wifiIp, FontSize::Tiny);
        D::drawText(s.wifiIp, static_cast<int16_t>(ml::kRightX - ipW), ml::kStatusTextY, FontSize::Tiny,
                    ml::kColorStatusText);
    }
    const int32_t ssidMaxW = ml::kRightX - ml::kStatusTextX - (ipW > 0 ? ipW + ml::kStatusGap : 0);
    if (s.wifiSsid[0] != '\0' && ssidMaxW > 0) {
        char line[kStatusFitCap];
        fitText(s.wifiSsid, line, sizeof(line), FontSize::Tiny, ssidMaxW);
        D::drawText(line, ml::kStatusTextX, ml::kStatusTextY, FontSize::Tiny, ml::kColorStatusText);
    }
}

// [S2c1] Годинник (FontSize::Clock, за ВЕРХОМ цифр) і дата під ним. Час недійсний — «--:--» і «Час невідомий».
void drawClockBlock() {
    const bool ok = s_clock.valid;
    // Цифри й дата центруються по осі блоку годинника (ml::kClockCenterX).
    const int32_t tw = UiFonts::textWidth(s_clock.time, FontSize::Clock);
    D::drawText(s_clock.time, static_cast<int16_t>(ml::kClockCenterX - tw / 2), ml::kClockFontY, FontSize::Clock,
                ok ? ml::kColorClock : ml::kColorClockDim);
    char date[sizeof(ClockView::date) + 8];
    fitText(s_clock.date, date, sizeof(date), FontSize::Small, ml::kClockBlockW);
    const int32_t dw = UiFonts::textWidth(date, FontSize::Small);
    D::drawText(date, static_cast<int16_t>(ml::kClockCenterX - dw / 2), ml::kDateY, FontSize::Small,
                ok ? ml::kColorDate : ml::kColorAmber);
}

// [S2c1] Погода: іконка за weather::category, температура (XLarge, праворуч), місто (Small), стан (Tiny).
// Немає міста — «--°» і «Місто не задано»; місто є, даних нема — «--°», назва міста й «Немає даних»;
// дані застаріли — усе приглушеним кольором. Мінус — ASCII '-' (U+2212 у шрифті немає).
constexpr size_t kWxFitCap = 64;

void drawWeatherBlock() {
    const WeatherView& w = s_wx;
    const bool show = w.set && w.valid;
    const bool dim  = !show || w.stale;
    const uint16_t cCity  = dim ? ml::kColorWxStale : ml::kColorWxCity;
    const uint16_t cExtra = dim ? ml::kColorWxStale : ml::kColorWxState;
    char line[kWxFitCap];

    // Верх блоку: місто (або «Місто не задано»).
    if (!w.set) {
        fitText(ua_text::kCityNotSet, line, sizeof(line), FontSize::Small, ml::kWeatherW);
        D::drawText(line, ml::kWeatherX, ml::kCityY, FontSize::Small, ml::kColorWxStale);
    } else {
        fitText(w.city, line, sizeof(line), FontSize::Small, ml::kWeatherW);
        D::drawText(line, ml::kWeatherX, ml::kCityY, FontSize::Small, cCity);
    }

    // Температура (XLarge) праворуч у рядку іконки.
    char temp[16];
    if (show) snprintf(temp, sizeof(temp), "%d%s", static_cast<int>(w.temp), ua_text::kDegree);
    else      snprintf(temp, sizeof(temp), "%s%s", ua_text::kTempUnknown, ua_text::kDegree);
    const int32_t tw = UiFonts::textWidth(temp, FontSize::XLarge);
    const int32_t tempX = ml::kRightX - tw;
    D::drawText(temp, static_cast<int16_t>(tempX), ml::kTempY, FontSize::XLarge,
                dim ? ml::kColorWxStale : ml::kColorTemp);

    const int16_t textX = static_cast<int16_t>(ml::kWeatherX + ml::kWeatherIconSize + 4);
    int32_t regionW = tempX - 4 - textX;   // ширина між іконкою й температурою
    if (regionW < 20) regionW = 20;
    const int16_t rowTop = ml::kWeatherIconY;

    if (!show) {
        // Місто задано, даних немає: підпис у зоні стану (іконки немає).
        if (w.set) {
            fitText(ua_text::kNoWeatherData, line, sizeof(line), FontSize::Tiny, tempX - 4 - ml::kWeatherX);
            D::drawText(line, ml::kWeatherX, static_cast<int16_t>(rowTop + (ml::kWeatherIconSize - dc::kFontTinyPx) / 2),
                        FontSize::Tiny, ml::kColorWxStale);
        }
        return;
    }

    drawWeatherIcon(static_cast<weather::Condition>(w.cond), w.isDay, w.stale, ml::kWeatherX, rowTop);

    // Стан праворуч від іконки: один рядок, а якщо не вміщається — у два (поділ за першим пробілом).
    const char* state = (w.cond < ua_text::kConditionCount) ? ua_text::kCondition[w.cond]
                                                            : ua_text::kCondition[ua_text::kConditionCount - 1];
    const char* sp = strchr(state, ' ');
    if (UiFonts::textWidth(state, FontSize::Tiny) <= regionW || sp == nullptr) {
        fitText(state, line, sizeof(line), FontSize::Tiny, regionW);
        D::drawText(line, textX, static_cast<int16_t>(rowTop + (ml::kWeatherIconSize - dc::kFontTinyPx) / 2),
                    FontSize::Tiny, cExtra);
    } else {
        char part[kWxFitCap];
        size_t n = static_cast<size_t>(sp - state);
        if (n >= sizeof(part)) n = sizeof(part) - 1;
        memcpy(part, state, n);
        part[n] = '\0';
        const int16_t y1 = static_cast<int16_t>(rowTop + (ml::kWeatherIconSize - 2 * dc::kFontTinyPx) / 2);
        fitText(part, line, sizeof(line), FontSize::Tiny, regionW);
        D::drawText(line, textX, y1, FontSize::Tiny, cExtra);
        fitText(sp + 1, line, sizeof(line), FontSize::Tiny, regionW);
        D::drawText(line, textX, static_cast<int16_t>(y1 + dc::kFontTinyPx), FontSize::Tiny, cExtra);
    }

    // Нижній рядок (Tiny): ліворуч вітер (стрілка напрямку + швидкість), праворуч тиск, між ними вологість.
    char wind[16];
    snprintf(wind, sizeof(wind), "%d,%d %s", w.windDm / 10, w.windDm % 10, ua_text::kUnitWind);   // десяткова кома
    const int32_t sw = UiFonts::textWidth(wind, FontSize::Tiny);
    drawWindArrow(static_cast<int16_t>(ml::kWeatherX + ml::kWindArrowW / 2),
                  static_cast<int16_t>(ml::kStateY + dc::kFontTinyPx / 2), ml::kWindArrowR, w.windDirDeg, cExtra);
    const int16_t windTextX = static_cast<int16_t>(ml::kWeatherX + ml::kWindArrowW + 3);
    D::drawText(wind, windTextX, ml::kStateY, FontSize::Tiny, cExtra);
    const int32_t windEnd = windTextX + sw;

    char pres[24];   // [S2b3] "760 мм рт. ст." = 20 байт UTF-8 + NUL (було 16 для «гПа»)
    if (w.pressureMm > 0)  snprintf(pres, sizeof(pres), "%u %s", static_cast<unsigned>(w.pressureMm), ua_text::kUnitPressure);
    else                   snprintf(pres, sizeof(pres), "%s %s", ua_text::kPressureUnknown, ua_text::kUnitPressure);
    const int32_t pw = UiFonts::textWidth(pres, FontSize::Tiny);
    const int32_t presX = ml::kRightX - pw;
    D::drawText(pres, static_cast<int16_t>(presX), ml::kStateY, FontSize::Tiny,
                (w.pressureMm > 0) ? cExtra : ml::kColorWxStale);

    char hum[8];
    snprintf(hum, sizeof(hum), "%u%%", static_cast<unsigned>(w.humidity));
    const int32_t hw = UiFonts::textWidth(hum, FontSize::Tiny);
    const int32_t gapL = windEnd + 4;
    const int32_t gapR = presX - 4;
    if (gapR - gapL >= hw) {   // не вміщається — не малюємо, щоб не наїжджати на сусідів
        D::drawText(hum, static_cast<int16_t>(gapL + (gapR - gapL - hw) / 2), ml::kStateY, FontSize::Tiny, cExtra);
    }
}

// [Prompt 32] Спливне вікно параметра звуку: підпис (Large) + значення великими цифрами.
// Знак +/- лише для параметрів зі знаком (тембр, баланс), і лише коли значення не нуль
// (як було в статус-рядку). Гучність — без знака.
// [P48b-фікс] Баланс береться з AppStateData::balanceDb (дБ, −20..+20, баланс активного входу),
// [P50b] AdjustTarget має лише Volume і Balance; legacy-цілі тембру / gain видалено.
void drawPopup(const AppStateData& s) {
    const char* label = "";
    int value = 0;
    bool signedValue = false;
    switch (s.popupTarget) {
        case AdjustTarget::Volume:  label = "Volume";    value = s.volume;  break;
        case AdjustTarget::Balance: label = "Balance";   value = s.balanceDb; signedValue = true; break;   // [P48b-фікс]
    }
    char buf[8];
    if (signedValue && value != 0) snprintf(buf, sizeof(buf), "%+d", value);
    else                           snprintf(buf, sizeof(buf), "%d", value);

    D::fillRect(c::kPopupX, c::kPopupY, c::kPopupW, c::kPopupH, c::kColorPopupBorder);
    D::fillRect(static_cast<int16_t>(c::kPopupX + c::kPopupBorder),
                static_cast<int16_t>(c::kPopupY + c::kPopupBorder),
                static_cast<int16_t>(c::kPopupW - 2 * c::kPopupBorder),
                static_cast<int16_t>(c::kPopupH - 2 * c::kPopupBorder), c::kColorPopupBg);

    const int32_t lw = UiFonts::textWidth(label, FontSize::Large);
    D::drawText(label, static_cast<int16_t>(c::kPopupX + (c::kPopupW - lw) / 2), c::kPopupLabelY,
                FontSize::Large, c::kColorPopupLabel);
    const int32_t dw = UiFonts::textWidth(buf, FontSize::Digits);
    D::drawText(buf, static_cast<int16_t>(c::kPopupX + (c::kPopupW - dw) / 2), c::kPopupDigitsY,
                FontSize::Digits, c::kColorPopupDigits);
}

// [Prompt 10] Текст і колір статусу потоку за StreamStatus.
// [Prompt 33] Раніше drawStreamStatus() малювала це окремим рядком (y=122); тепер це лише
// вибір тексту й кольору (тексти БЕЗ змін), а малює обʼєднаний рядок drawRadio().
//   Idle → сірий "Stopped"; Connecting/Buffering/Reconnecting → жовті;
//   Playing → зелений "Playing"; Error → червоний "Error".
// Wi-Fi статус показується окремою іконкою (не змінюється).
const char* statusText(StreamStatus st, uint16_t& color) {
    switch (st) {
        case StreamStatus::Idle:
            color = c::kColorDim;
            return "Stopped";
        case StreamStatus::Connecting:
            color = c::kColorAccent;
            return "Connecting...";
        case StreamStatus::Buffering:
            color = c::kColorAccent;
            return "Buffering...";
        case StreamStatus::Playing:
            color = c::kColorOk;
            return "Playing";
        case StreamStatus::Error:
            color = c::kColorBad;
            return "Error";
        case StreamStatus::Reconnecting:
            color = c::kColorAccent;
            return "Reconnecting...";
        default:
            color = c::kColorDim;
            return "?";
    }
}

// [Prompt 33] Обʼєднаний рядок екрана Radio: метадані, коли потік грає й вони є; інакше статус.
// Повертає вказівник або на s.trackTitle, або на літерал; color — колір відповідного тексту.
const char* radioInfoLine(const AppStateData& s, uint16_t& color) {
    if (s.streamStatus == StreamStatus::Playing && s.trackTitle[0] != '\0') {
        color = c::kColorTrack;   // [Prompt 35] світліше за kColorDim
        return s.trackTitle;
    }
    return statusText(s.streamStatus, color);
}

// ---------------------------------------------------------------------------
// Екрани
// ---------------------------------------------------------------------------
void drawStandby() {
    D::drawIcon(IconId::Standby, c::kStandbyIconX, c::kStandbyIconY, c::kColorDim);
    drawCentered("Standby", c::kStandbyCaptionY, FontSize::Tiny, c::kColorDim);
}

// [S2c1] Основний екран 320×240 (Radio / ExternalInput; Player — заглушка). Порядок шарів: смуга стану,
// годинник | погода, розділювач, тег джерела, назва, рядок треку / «Немає зв'язку», VU, гучність (+ мʼют).
void drawMain(const AppStateData& s, int vuLitL, int vuLitR, int peakL, int peakR) {
    const bool radio  = (s.mode == Mode::Radio);
    const bool player = (s.mode == Mode::ExternalInput) && (s.inputIndex == ml::kPlayerInputIndex);

    drawStatusBar(s);
    drawClockBlock();
    D::drawLine(ml::kSepX, ml::kBlockTop, ml::kSepX, ml::kBlockBottom, ml::kColorDivider);
    drawWeatherBlock();
    D::drawLine(ml::kSideMargin, ml::kDividerY, static_cast<int16_t>(ml::kRightX - 1), ml::kDividerY,
                ml::kColorDivider);

    char name[kInputFitCap];   // [Prompt 23b]
    if (radio) {
        // Тег = назва входу («WiFi Radio»); назва станції й обʼєднаний рядок «метадані / статус» — marquee.
        fitText(inputName(s), name, sizeof(name), FontSize::Tiny, ml::kContentW);
        D::drawText(name, ml::kSideMargin, ml::kTagY, FontSize::Tiny, ml::kColorTag);
        marqueeDraw(s_station, ml::kStationY, FontSize::XLarge, ml::kColorStation);
        uint16_t infoColor = c::kColorTrack;
        (void)radioInfoLine(s, infoColor);
        marqueeDraw(s_track, ml::kTrackY, FontSize::Small, infoColor);
    } else {
        D::drawText(ua_text::kSourceExternal, ml::kSideMargin, ml::kTagY, FontSize::Tiny, ml::kColorTag);
        fitText(inputName(s), name, sizeof(name), FontSize::XLarge, ml::kContentW);
        const uint16_t color = (s.inputIndex >= 1 && s.inputIndex <= c::kColorInputNameCount)
                                   ? c::kColorInputName[s.inputIndex - 1]
                                   : dc::kColorFg;   // запасний колір, якщо індекс поза таблицею
        D::drawText(name, ml::kSideMargin, ml::kStationY, FontSize::XLarge, color);
        if (player) {
            D::drawText(ua_text::kNoLink, ml::kSideMargin, ml::kTrackY, FontSize::Small, ml::kColorAmber);
        }
    }

    drawVu(vuLitL, vuLitR, peakL, peakR);

    char vol[8];
    snprintf(vol, sizeof(vol), "%d", static_cast<int>(s.volume));
    const int32_t vw = UiFonts::textWidth(vol, FontSize::XLarge);
    // «Vol» і число — одна група (XLarge), відцентрована між кінцем VU і правим краєм контенту.
    const int32_t lw = UiFonts::textWidth("Vol", FontSize::XLarge);
    const int32_t zoneL = ml::kVuBarX + ml::kVuUsedW;
    int32_t gx = zoneL + (ml::kRightX - zoneL - (lw + ml::kVolLabelGap + vw)) / 2;
    if (gx < zoneL + 4) gx = zoneL + 4;
    D::drawText("Vol", static_cast<int16_t>(gx), ml::kVolY, FontSize::XLarge,
                s.mute ? ml::kColorVolumeMute : ml::kColorVuLabel);
    D::drawText(vol, static_cast<int16_t>(gx + lw + ml::kVolLabelGap), ml::kVolY, FontSize::XLarge,
                s.mute ? ml::kColorVolumeMute : ml::kColorVolume);
    // Мʼют: слот 0 смуги іконок у правому верхньому куті блоку джерела (рядок тегу).
    if (s.mute) D::drawIcon(IconId::Mute, ml::kVolMuteIconX, ml::kVolMuteIconY, c::kColorAccent);
}

void drawStationList(const AppStateData& s) {
    D::drawText("Stations", c::kMargin, c::kListTitleY, FontSize::Small, c::kColorDim);
    D::drawLine(0, c::kListLineY, c::kW - 1, c::kListLineY, c::kColorDim);

    // [Prompt 11] Реальні назви зі StationStore.
    const size_t count = StationStore::count();
    if (count == 0) {
        drawCentered("No stations", c::kPlaceholderY, FontSize::Large, c::kColorDim);
        return;
    }

    const int32_t maxW = c::kW - c::kListTextX - c::kMargin;
    const uint16_t sel = s.menuSelection;
    const uint16_t first = (sel >= c::kListRows) ? static_cast<uint16_t>(sel - c::kListRows + 1) : 0;
    Station st;
    char text[sizeof(Station::name) + 8];
    for (uint8_t i = 0; i < c::kListRows; ++i) {
        const uint32_t idx = static_cast<uint32_t>(first) + i;
        const int16_t y = static_cast<int16_t>(c::kListFirstY + i * c::kListRowH);
        if (idx == sel) D::fillRect(0, y, c::kW, c::kListRowH, c::kColorSelectionBg);
        if (idx >= count || !StationStore::get(idx, st)) continue;  // порожній рядок
        fitText(st.name, text, sizeof(text), FontSize::Small, maxW);
        D::drawText(text, c::kListTextX, static_cast<int16_t>(y + c::kListTextDy), FontSize::Small,
                    dc::kColorFg);
    }
}

// [Prompt 49b] Екран «EQ + Balance»: шапка (назва екрана й назва активного входу), вертикальні
// стовпчиків (смуги EQ і баланс) від спільної нульової лінії. Значення зі знаком над кінцем додатного
// стовпчика або під кінцем від'ємного; 0 — лише лінія, значення «0» над нею. Вибраний стовпчик — підсвітка
// колонки, яскравіша заливка й більше значення. Усі координати — у config/eq_screen_config.h (eqc::).
namespace eqc = eq_screen_cfg;

// Підпис смуги ([E4]: формується з eq_cfg::kBandFreqHz, ручного списку немає): <1000 Гц — число (31, 125), ≥1000 — з «k»
// (1k, 10k, 16k), нецілі тисячі — з десятковою ТОЧКОЮ без зайвих нулів (2.8k, 5.6k, 1.5k).
void formatBandLabel(uint16_t hz, char* out, size_t cap) {
    if (hz < 1000u)                 snprintf(out, cap, "%u", static_cast<unsigned>(hz));
    else if (hz % 1000u == 0u)      snprintf(out, cap, "%uk", static_cast<unsigned>(hz / 1000u));
    else                            snprintf(out, cap, "%u.%uk", static_cast<unsigned>(hz / 1000u),
                                             static_cast<unsigned>((hz % 1000u) / 100u));
}

// Значення зі знаком: 0 — без знака, "+3", "-5" (ASCII-мінус; шрифт не має U+2212).
void formatSignedDb(int v, char* out, size_t cap) {
    if (v == 0) snprintf(out, cap, "0");
    else        snprintf(out, cap, "%+d", v);
}

// [E4] Значення колонки (смуга EQ або баланс): кегль і знак підбираються за шириною колонки eqc::kColW (колонка 27 px).
// Порядок: вибране — Small, решта — Tiny; якщо текст із запасом eqc::kValueSidePadPx не влазить — Tiny; якщо й так не влазить
// — додатне без «+» (від'ємне лишається зі знаком «-»). Повертає кегль; out — готовий рядок.
FontSize fitBandValue(int v, bool selected, char* out, size_t cap) {
    formatSignedDb(v, out, cap);
    const int32_t maxW = eqc::kColW - 2 * eqc::kValueSidePadPx;
    if (selected && UiFonts::textWidth(out, FontSize::Small) <= maxW) {
        return FontSize::Small;
    }
    if (v > 0 && UiFonts::textWidth(out, FontSize::Tiny) > maxW) {
        snprintf(out, cap, "%d", v);   // «+18» не влазить у Tiny: без «+»
    }
    return FontSize::Tiny;
}

void drawEqBalance(const AppStateData& s) {
    // --- Шапка: заголовок ліворуч, назва активного входу праворуч (обрізається, щоб не налізти).
    const char* title = "EQ + Balance";
    D::drawText(title, c::kMargin, eqc::kHeaderTitleY, FontSize::Small, dc::kColorFg);
    const int32_t titleW = UiFonts::textWidth(title, FontSize::Small);
    char name[kInputFitCap];
    const int32_t nameMaxW = c::kW - 2 * c::kMargin - titleW - eqc::kHeaderNameGap;
    if (nameMaxW > 0) {
        fitText(inputName(s), name, sizeof(name), FontSize::Small, nameMaxW);
        const int32_t nameW = UiFonts::textWidth(name, FontSize::Small);
        D::drawText(name, static_cast<int16_t>(c::kW - c::kMargin - nameW), eqc::kHeaderTitleY,
                    FontSize::Small, eqc::kColorHeaderName);
    }
    D::drawLine(0, eqc::kHeaderLineY, c::kW - 1, eqc::kHeaderLineY, c::kColorDim);

    // Вибраний індекс захищаємо: поза діапазоном — перша смуга (логіка AppController не має такого стану).
    const uint8_t sel = (s.eqSelected < eqc::kColumnCount) ? s.eqSelected : 0;

    // --- Підсвітка вибраної колонки (під стовпчиками, над фоном).
    const bool selIsBal = (sel == eqc::kBalanceColumn);
    D::fillRect(eqc::colX(sel), eqc::kAreaTop, eqc::kColW, static_cast<int16_t>(eqc::kAreaBottom - eqc::kAreaTop),
                selIsBal ? eqc::kColorBalSel : eqc::kColorEqSel);

    // --- Нульова лінія через усі колонки і розділювач EQ / Bal.
    D::drawLine(eqc::kLeft, eqc::kZeroY, static_cast<int16_t>(eqc::kLeft + eqc::kTotalW - 1), eqc::kZeroY,
                eqc::kColorZero);
    D::drawLine(eqc::kDividerX, eqc::kAreaTop, eqc::kDividerX, eqc::kAreaBottom, eqc::kColorDivider);

    for (uint8_t i = 0; i < eqc::kColumnCount; ++i) {
        const bool isBal    = (i == eqc::kBalanceColumn);
        const bool selected = (i == sel);
        const int  value    = isBal ? s.balanceDb : s.eqGainsDb[i];
        const int  fullDb   = isBal ? eqc::kBalFullDb : eqc::kEqFullDb;
        const int16_t colX  = eqc::colX(i);

        // Висота бару: |value| / fullDb × kHalfH з округленням; обмежено піввисотою.
        int absV = value < 0 ? -value : value;
        if (absV > fullDb) absV = fullDb;
        const int h = (absV * eqc::kHalfH + fullDb / 2) / fullDb;

        // --- Стовпчик від нульової лінії: вгору для додатних, вниз для від'ємних; 0 — без бару.
        if (h > 0) {
            const uint16_t barColor = isBal ? (selected ? eqc::kColorBalBar : eqc::kColorBalBarDim)
                                            : (selected ? eqc::kColorEqBar : eqc::kColorEqBarDim);
            const int16_t barX = static_cast<int16_t>(colX + (eqc::kColW - eqc::kBarW) / 2);
            const int16_t barY = (value > 0) ? static_cast<int16_t>(eqc::kZeroY - h)
                                             : static_cast<int16_t>(eqc::kZeroY + 1);
            D::fillRect(barX, barY, eqc::kBarW, static_cast<int16_t>(h), barColor);
        }

        // --- Значення: над кінцем додатного (і над нульовою лінією для 0), під кінцем від'ємного.
        char val[8];
        const FontSize valFont = fitBandValue(value, selected, val, sizeof(val));   // [E4] за шириною колонки
        const int16_t  valH    = (valFont == FontSize::Small) ? eqc::kValueSmallPx : eqc::kValueTinyPx;
        const uint16_t valColor = isBal ? (selected ? eqc::kColorBalBar : eqc::kColorBalValue)
                                        : (selected ? eqc::kColorValueSel : eqc::kColorValueDim);
        const int32_t valW = UiFonts::textWidth(val, valFont);
        const int16_t valX = static_cast<int16_t>(colX + (eqc::kColW - valW) / 2);
        const int16_t valY = (value < 0) ? static_cast<int16_t>(eqc::kZeroY + h + eqc::kValueGap)
                                         : static_cast<int16_t>(eqc::kZeroY - h - eqc::kValueGap - valH);
        D::drawText(val, valX, valY, valFont, valColor);
    }

    // --- Підписи внизу: частоти смуг і «Bal».
    for (uint8_t i = 0; i < eqc::kColumnCount; ++i) {
        const bool isBal    = (i == eqc::kBalanceColumn);
        const bool selected = (i == sel);
        char label[12];
        if (isBal) snprintf(label, sizeof(label), "Bal");
        else       formatBandLabel(eq_cfg::kBandFreqHz[i], label, sizeof(label));
        const uint16_t labelColor = selected ? eqc::kColorLabelSel
                                             : (isBal ? eqc::kColorBalValue : eqc::kColorLabelDim);
        const int32_t labelW = UiFonts::textWidth(label, FontSize::Tiny);
        const int16_t labelX = static_cast<int16_t>(eqc::colX(i) + (eqc::kColW - labelW) / 2);
        D::drawText(label, labelX, eqc::kLabelY, FontSize::Tiny, labelColor);
    }
}

void drawPlaceholder(const char* text) {
    drawCentered(text, c::kPlaceholderY, FontSize::Large, dc::kColorFg);
}

// [Prompt 12] Екран Mode::WifiSetup: до чого підключитись і що відкрити на телефоні.
void drawWifiSetup(const AppStateData& s) {
    drawCentered("Setup Wi-Fi", c::kWifiSetupTitleY, FontSize::Large, dc::kColorFg);
    drawCentered(s.wifiSsid[0] ? s.wifiSsid : defaults::kApSsid, c::kWifiSetupSsidY,
                 FontSize::Small, c::kColorAccent);
    char url[32];
    snprintf(url, sizeof(url), "http://%s", s.wifiIp[0] ? s.wifiIp : "192.168.4.1");
    drawCentered(url, c::kWifiSetupIpY, FontSize::Small, c::kColorDim);
    // [Prompt 28] Підказка: OK (кнопка / енкодер / IR) запускає офлайн-режим.
    drawCentered("OK: work offline", c::kWifiSetupHintY, FontSize::Small, c::kColorDim);
}

// [Prompt 15] Екран Mode::IrLearn: статус (Small) над назвою дії (Large), підказка під нею.
// Лише інформаційний: підтвердження/скасування робиться з вебу. Статус «Waiting» дає
// «Press remote button for» просто над назвою дії - фраза читається разом з нею.
void drawIrLearn(const AppStateData& s) {
    char line[40];
    const char* hint = "";
    uint16_t color = c::kColorDim;

    switch (s.irLearnStatus) {
        case IrLearnStatus::Waiting:
            strlcpy(line, "Press remote button for", sizeof(line));
            color = c::kColorAccent;
            break;
        case IrLearnStatus::Confirm:
            strlcpy(line, "Press again to confirm", sizeof(line));
            color = c::kColorAccent;
            break;
        case IrLearnStatus::Success:
            strlcpy(line, "Learned!", sizeof(line));
            color = c::kColorOk;
            break;
        case IrLearnStatus::Timeout:
            strlcpy(line, "Timeout", sizeof(line));
            color = c::kColorBad;
            break;
        case IrLearnStatus::Conflict:
            snprintf(line, sizeof(line), "Already used by %s",
                     action_names::name(s.irLearnConflictWith));
            hint = "Resolve on web";
            color = c::kColorBad;
            break;
        case IrLearnStatus::Idle:
        default:
            line[0] = '\0';  // кадр-два між зміною режиму й першим статусом
            break;
    }

    char text[48];  // fitText: cap >= довжина джерела + 8
    fitText(line, text, sizeof(text), FontSize::Small, c::kContentW);
    drawCentered(text, c::kIrStatusY, FontSize::Small, color);
    drawCentered(action_names::name(s.irLearnTarget), c::kIrActionY, FontSize::Large,
                 dc::kColorFg);
    if (hint[0] != '\0') {
        drawCentered(hint, c::kIrHintY, FontSize::Small, c::kColorDim);
    }
}

// [Prompt 16] Екран Mode::OtaUpdate: великий відсоток по центру + смуга; під час прошивки
// дивляться на пристрій, а не на телефон. Три крапки ASCII, не «…»: U+2026 немає в шрифті
// (ASCII/Latin-1/Latin Ext-A/кирилиця).
void drawOtaUpdate(const AppStateData& s) {
    const uint8_t pct = s.otaProgress > 100 ? 100 : s.otaProgress;
    drawCentered("Updating...", c::kOtaTitleY, FontSize::Large, dc::kColorFg);
    char num[8];
    snprintf(num, sizeof(num), "%u%%", static_cast<unsigned>(pct));
    drawCentered(num, c::kOtaPercentY, FontSize::Large, c::kColorAccent);
    D::drawRect(c::kOtaBarX, c::kOtaBarY, c::kOtaBarW, c::kOtaBarH, c::kColorDim);
    const int16_t inner = static_cast<int16_t>(c::kOtaBarW - 2);
    const int16_t fillW = static_cast<int16_t>((static_cast<int32_t>(inner) * pct) / 100);
    if (fillW > 0) {
        D::fillRect(c::kOtaBarX + 1, c::kOtaBarY + 1, fillW, c::kOtaBarH - 2, c::kColorAccent);
    }
    drawCentered("Do not power off", c::kOtaHintY, FontSize::Small, c::kColorDim);
}

// ---------------------------------------------------------------------------
// Підсвітка при вході/виході зі Standby
// ---------------------------------------------------------------------------
void handleModeChange(Mode mode) {
    if (s_modeKnown && mode == s_prevMode) return;

    const bool wasStandby = s_modeKnown && s_prevMode == Mode::Standby;
    const bool isStandby  = mode == Mode::Standby;
    if (isStandby && !wasStandby) {
        s_savedBrightness = D::brightness();
        D::setBrightness(c::kStandbyBrightnessPercent);
    } else if (wasStandby && !isStandby) {
        D::setBrightness(s_savedBrightness);
    }
    // Нова сцена -> marquee починається спочатку.
    s_station.seeded = false;
    s_track.seeded   = false;
    s_prevMode  = mode;
    s_modeKnown = true;
}

// ---------------------------------------------------------------------------
// FrameCallback
// ---------------------------------------------------------------------------
bool frame() {
    const uint32_t now = millis();
    const AppStateData s = AppState::snapshot();   // один знімок на кадр

    handleModeChange(s.mode);
    wifiBarsUpdate(s);   // [Prompt 36] гістерезис іконки Wi-Fi; зміна rssi вже дає зміну знімка

    // [S2c1] Основний екран = Radio / ExternalInput (не під час «тихого» перезапуску): 240 рядків, решта — 170.
    const bool mainScreen = !s.restarting && (s.mode == Mode::Radio || s.mode == Mode::ExternalInput);

    // full — потрібен повний вивід кадру (змінилось щось поза VU / marquee); інакше лише рядки [rowMin, rowMax).
    bool full = !s_havePrev || memcmp(&s, &s_prev, sizeof(s)) != 0;
    if (full) {
        s_prev = s;
        s_havePrev = true;
    }
    if (static_cast<int>(s_wifiBars) != s_wifiBarsDrawn) {   // гістерезис змінив іконку без зміни знімка
        s_wifiBarsDrawn = s_wifiBars;
        full = true;
    }
    int rowMin = 0x7FFF;
    int rowMax = -1;
    auto markRows = [&rowMin, &rowMax](int y, int h) {
        if (y < rowMin) rowMin = y;
        if (y + h > rowMax) rowMax = y + h;
    };

    // [S2c1] Годинник і погода: неблокуючі знімки (раз на kClockPollMs / kWeatherPollMs), без мережі.
    if (mainScreen) {
        if (clockUpdate(now))   full = true;
        if (weatherUpdate(now)) full = true;
    }

    // [Prompt 17] VU: лише Radio + Playing, інакше рівні 0. read() == false (даних нема) теж дає 0.
    // [Prompt 34] Пік утримується в UI; без даних пік скидається.
    // [S2c1] Player: VU порожні (без базових сегментів); зміна рівнів позначає лише смугу VU.
    int litL = 0;
    int litR = 0;
#if ENABLE_VU
    const bool isPlayer = (s.mode == Mode::ExternalInput) && (s.inputIndex == ml::kPlayerInputIndex);
    if (mainScreen && !isPlayer) {
        bool vuValid = false;
        if (s.mode == Mode::Radio && s.streamStatus == StreamStatus::Playing) {
            float vuL = 0.0f;
            float vuR = 0.0f;
            if (s_vu.read(vuL, vuR)) {
                litL = vuToSegments(vuL);
                litR = vuToSegments(vuR);
                vuValid = true;
            }
        }
#if LINE_PATH_ENABLE
        // [Prompt 48d] Зовнішні входи (inputIndex >= 1): рівні з line_path (VuSourceLinePath).
        else if (s.mode == Mode::ExternalInput && s.inputIndex >= 1) {
            float vuL = 0.0f;
            float vuR = 0.0f;
            if (s_vuLine.read(vuL, vuR)) {
                litL = vuToSegments(vuL);
                litR = vuToSegments(vuR);
                vuValid = true;
            }
        }
#endif
        // Базовий рівень: перші kVuMinSegments сегментів світяться завжди (тиша, пауза, обрив).
        if (litL < ml::kVuMinSegments) litL = ml::kVuMinSegments;
        if (litR < ml::kVuMinSegments) litR = ml::kVuMinSegments;
        if (vuValid) {
            vuPeakUpdate(s_peakL, litL, now);
            vuPeakUpdate(s_peakR, litR, now);
        } else {
            vuPeakReset(s_peakL);
            vuPeakReset(s_peakR);
        }
    } else {
        vuPeakReset(s_peakL);
        vuPeakReset(s_peakR);
    }
#endif
    if (litL != s_vuLitL || litR != s_vuLitR || s_peakL.seg != s_vuPeakDrawnL ||
        s_peakR.seg != s_vuPeakDrawnR) {
        s_vuLitL = litL;
        s_vuLitR = litR;
        s_vuPeakDrawnL = s_peakL.seg;
        s_vuPeakDrawnR = s_peakR.seg;
        if (mainScreen) markRows(ml::kVuLY, ml::kVuBandH);
    }

    // [Prompt 32] Спливне вікно: нова подія (popupSeq змінився) запускає/продовжує показ;
    // поза Radio/ExternalInput (і під час перезапуску) вікно ховається й не повертається саме.
    const bool popupMode = mainScreen;
    if (!s_popupSeqKnown) {
        s_popupSeqKnown = true;                 // перший кадр: не вважати старе значення подією
        s_popupSeqSeen  = s.popupSeq;
    } else if (s.popupSeq != s_popupSeqSeen) {
        s_popupSeqSeen = s.popupSeq;
        if (popupMode && !s.popupDismissed) {   // [Prompt 49d] «сховати» (рівень) сильніше за подію «показати» в тому ж кадрі
            s_popupActive  = true;
            s_popupUntilMs = now + c::kPopupTimeoutMs;
        }
    }
    if (!popupMode) {
        s_popupActive = false;
    } else if (s_popupActive && s.popupDismissed) {
        s_popupActive = false;                  // [Prompt 49d] кнопка / клік енкодера / IR — сховати одразу
        full = true;                            // перемалювати кадр без вікна
    } else if (s_popupActive && static_cast<int32_t>(now - s_popupUntilMs) >= 0) {
        s_popupActive = false;
        full = true;                            // перемалювати кадр без вікна
    }

    if (mainScreen && s.mode == Mode::Radio) {
        // [Prompt 35] Назва станції — XLarge; [S2c1] marquee лише позначає свої рядки (часткова передача).
        if (marqueeUpdate(s_station, s.stationName[0] ? s.stationName : "No station", FontSize::XLarge, now)) {
            markRows(ml::kStationY, dc::kFontXLargePx);
        }
        // [Prompt 33] Зміна станції — рядок «метадані / статус» починається спочатку (навіть якщо
        // текст не змінився). Зміну самого тексту (метадані ↔ статус, нові метадані) marqueeUpdate
        // ловить сам через strcmp.
        if (!s_trackStationKnown || s.stationIndex != s_trackStation) {
            s_trackStationKnown = true;
            s_trackStation      = s.stationIndex;
            s_track.seeded      = false;
        }
        uint16_t infoColor = c::kColorTrack;   // колір тут не потрібен, лише текст
        const char* info = radioInfoLine(s, infoColor);
        if (marqueeUpdate(s_track, info, FontSize::Small, now)) {   // [S2c1] Small (було Large)
            markRows(ml::kTrackY, dc::kFontSmallPx);
        }
    }

    if (!full && rowMax < 0) return false;   // панель лишається як була

    D::setFullPanel(mainScreen);              // [S2c1] 240 рядків для основного екрана, 170 для решти
    D::fillScreen(dc::kColorBg);
    if (s.restarting) {  // [Prompt 28] «тихий» перезапуск: один рядок замість будь-якого екрана
        drawPlaceholder("Restarting...");  // три крапки ASCII: U+2026 немає у шрифті
        return true;
    }
    switch (s.mode) {
        case Mode::Standby:       drawStandby();                 break;
        case Mode::Radio:
        case Mode::ExternalInput: drawMain(s, litL, litR, s_peakL.seg, s_peakR.seg); break;   // [S2c1]
        case Mode::Menu:
            if (s.menuContext == MenuContext::StationList)    drawStationList(s);
            else if (s.menuContext == MenuContext::EqBalance) drawEqBalance(s);   // [Prompt 49a/49b]
            else                                              drawPlaceholder("Menu (TODO)");
            break;
        case Mode::IrLearn:       drawIrLearn(s);                break;   // [Prompt 15]
        case Mode::WifiSetup:     drawWifiSetup(s);              break;
        case Mode::OtaUpdate:     drawOtaUpdate(s);              break;   // [Prompt 16]
        default:                  drawPlaceholder("?");          break;
    }
    if (s_popupActive) drawPopup(s);   // [Prompt 32] останній шар кадру
    // [S2c1] Змінились лише VU / marquee: передати тільки їхні рядки (буфер перемальовано повністю).
    if (mainScreen && ml::kPartialPush && !full && rowMax > rowMin) {
        D::setPushBand(static_cast<int16_t>(rowMin), static_cast<int16_t>(rowMax - rowMin));
    }
    return true;
}

}  // namespace

bool UiScreens::begin() {
    s_havePrev  = false;
    s_modeKnown = false;
    s_station.seeded = false;
    s_track.seeded   = false;
    s_trackStationKnown = false;   // [Prompt 33]
    s_vuLitL = 0;
    s_vuLitR = 0;
    s_vuPeakDrawnL = 0;   // [Prompt 34]
    s_vuPeakDrawnR = 0;
#if ENABLE_VU
    vuPeakReset(s_peakL);
    vuPeakReset(s_peakR);
#endif
    // [S2c1] Знімки годинника / погоди: нульові (valid = false) -> перший же кадр вважається зміною.
    s_clock = ClockView();
    s_wx    = WeatherView();
    s_clockPolled   = false;
    s_weatherPolled = false;
    s_wifiBarsDrawn = -1;
    s_popupSeqKnown = false;   // [Prompt 32]
    s_popupActive   = false;
    DisplayManager::setFrameCallback(&frame);
    return true;
}
