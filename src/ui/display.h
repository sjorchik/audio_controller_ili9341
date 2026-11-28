#pragma once

// DisplayManager: ILI9341 240×320 (у проєкті горизонтально, 320×240) через LovyanGFX.
// [D1] Екрани малюються в ОБЛАСТІ 320×display_cfg::kLegacyViewportH (верхні 170 рядків);
// width()/height() повертають саме її. Решта рядків панелі лишається чорною.
//
// Архітектура:
//  - малювання лише у власні спрайти RGB565 320×240 у PSRAM (2 буфери); на панель
//    виводиться лише область екранів (перші kLegacyViewportH рядків);
//  - весь цикл кадру виконується в ОДНІЙ задачі FreeRTOS (taskLoop);
//  - примітиви малюють у «задній» буфер, present() виводить його на панель і
//    міняє буфери місцями. Після present() вміст нового заднього буфера
//    НЕВИЗНАЧЕНИЙ: кадр малюється повністю (починаючи з fillScreen).
//
// ПОТОКОВА МОДЕЛЬ: примітиви малювання й present() НЕ потокобезпечні. Їх слід
// викликати лише з display-задачі (з FrameCallback або з taskLoop). Інші
// задачі можуть викликати лише setBrightness()/brightness()/isReady()/
// lastError*()/setFlipped()/isFlipped(). Реальний контент (екрани) зʼявиться
// через FrameCallback.
//
// DisplayManager не читає EventBus і не знає про екрани.

#include <stdint.h>

#include "config/display_config.h"
#include "ui/fonts.h"
#include "ui/icons.h"

// Результат останньої спроби begin().
enum class DisplayError : uint8_t {
    None,                // усе гаразд
    NotStarted,          // begin() ще не викликали
    NoPsram,             // PSRAM не знайдено
    ResourceInitFailed,  // не ініціалізовано шрифти/іконки
    BacklightInitFailed, // ledcAttach() для BLK не вдався
    PanelInitFailed,     // init() панелі не вдався або розмір ≠ 320×240
    SpriteAllocFailed,   // не вдалося виділити буфери кадру в PSRAM
    TaskCreateFailed,    // не створено задачу малювання
};

class DisplayManager {
public:
    // Викликається кожен період кадру з display-задачі. Малює кадр примітивами
    // нижче й повертає true, якщо кадр треба вивести (present() викличе задача).
    // Повертає false — панель лишається як була. Встановлюється до або після begin().
    using FrameCallback = bool (*)();

    // Ініціалізація: PSRAM-перевірка, шрифти, іконки, підсвітка (вимкнена), панель
    // (апаратний скид через RST), буфери, задача. Повторний виклик після успіху
    // повертає true. Підсвітка вмикається після виводу ПЕРШОГО кадру.
    static bool begin();

    // Тіло задачі малювання. Не викликати вручну: його запускає begin().
    static void taskLoop();

    static DisplayError lastError();
    static const char* lastErrorName();
    static bool isReady();

    // Підсвітка, 0..100 %. Безпечно з будь-якої задачі. До першого кадру значення
    // лише запамʼятовується. Лінійна залежність яскравості від шпаруватості.
    static void setBrightness(uint8_t percent);
    static uint8_t brightness();

    static void setFrameCallback(FrameCallback callback);

    // Орієнтація: false = kRotationNormal, true = розвернуто на 180°
    // (kRotationFlipped). Безпечно з будь-якої задачі й до begin(): запит лише
    // запамʼятовується, а поворот панелі виконує display-задача в наступному
    // періоді кадру: очищує всю панель чорним (щоб у «невикористаній» смузі не
    // лишилось старої картинки) і повторно виводить останній кадр. Розміри не
    // змінюються, координати екранів переписувати не треба.
    static void setFlipped(bool flipped);
    static bool isFlipped();

    // Розміри ОБЛАСТІ ЕКРАНІВ (320×kLegacyViewportH), не всієї панелі.
    static int16_t width();
    static int16_t height();

    // [S2c1] Висота виводу. false (за замовчуванням) = область екранів kLegacyViewportH рядків (320×170);
    // true = вся панель kPanelH рядків (320×240; основний екран). Лише з display-задачі, до малювання
    // кадру (fillScreen() заливає рівно стільки рядків, скільки виводиться). При переході true -> false
    // нижня смуга панелі очищується чорним ОДИН раз при наступному present(). width()/height() НЕ
    // змінюються (лишаються розмірами «старої» області 320×kLegacyViewportH). У DISPLAY_DEMO — без дії.
    static void setFullPanel(bool full);
    static bool isFullPanel();

    // [S2c1] Часткова передача: наступний present() виведе на панель лише рядки [y, y + h) заднього
    // буфера (буфер при цьому має бути ПОВНІСТЮ перемальований, а решта кадру — ідентична тому, що вже
    // на панелі). Діє лише на один present(); ігнорується (виводиться весь кадр) при зміні орієнтації,
    // при переході висоти виводу або якщо діапазон поза межами. Лише з display-задачі.
    static void setPushBand(int16_t y, int16_t h);

    // --- Примітиви (лише з display-задачі; координати в пікселях, 0,0 = лівий верх) ---
    // Заливає область екранів (width() × height()); нижня смуга панелі не чіпається.
    static void fillScreen(uint16_t color);
    static void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    static void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    static void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);

    // Текст UTF-8, (x, y) — лівий верхній кут. Перша версія — прозоре тло,
    // друга — тло кольору bg (стирає попередній напис у межах символів).
    static void drawText(const char* utf8, int16_t x, int16_t y, FontSize size, uint16_t fg);
    static void drawText(const char* utf8, int16_t x, int16_t y, FontSize size, uint16_t fg,
                         uint16_t bg);

    // Іконка kIconSize×kIconSize, (x, y) — лівий верхній кут. Вся робота зі
    // спрайтом (drawBitmap) — тут; UiIcons дає лише маску.
    static void drawIcon(IconId id, int16_t x, int16_t y,
                         uint16_t color = display_cfg::kColorFg);

    // Вивести готовий задній буфер на панель і поміняти буфери місцями.
    static void present();
};
