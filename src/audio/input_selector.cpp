// Реле-селектор зовнішніх входів (Computer / Aux). [Prompt 48a]; [D2a] нова полярність і мапінг входів.
// Інтерфейс і правила використання — у audio/input_selector.h.

#include "audio/input_selector.h"

#include <Arduino.h>
#include <driver/gpio.h>

#include "config/input_selector_config.h"
#include "config/pins.h"

namespace {

namespace cfg = input_selector_cfg;

bool s_ready = false;
uint8_t s_current = cfg::kInputComputer;  // реле вимкнено після begin()

// Рівень піна для стану реле: relayOn = true -> реле увімкнено (Aux).
inline uint8_t levelFor(bool relayOn) {
    return (relayOn == cfg::kRelayActiveHigh) ? HIGH : LOW;
}

}  // namespace

namespace input_selector {

bool begin() {
    if (s_ready) {
        return true;
    }
    // [D2a] Рівень «вимкнено» (Computer) спершу пишемо драйвером GPIO, а НЕ digitalWrite(): до pinMode()
    // Arduino-обгортка друкує «IO 1 is not set as GPIO». Регістр виходу тримає рівень, тож після pinMode(OUTPUT)
    // реле одразу в Computer, без проміжного стану (зовнішній pulldown на драйвері реле тримає його вимкненим
    // до цього моменту). Після pinMode рівень дублюється для певності.
    gpio_set_level(static_cast<gpio_num_t>(pins::kInputRelay), levelFor(false));
    pinMode(pins::kInputRelay, OUTPUT);
    digitalWrite(pins::kInputRelay, levelFor(false));
    s_current = cfg::kInputComputer;
    s_ready = true;
    Serial.printf("[SEL] relay pin=%d activeHigh=%d -> Computer\n", pins::kInputRelay,
                  cfg::kRelayActiveHigh ? 1 : 0);
    return true;
}

bool select(uint8_t logicalInput) {
    if (!s_ready) {
        return false;
    }
    if (logicalInput != cfg::kInputComputer && logicalInput != cfg::kInputAux) {
        return false;  // 0 (радіо), 3 (Плеєр) й будь-що інше: реле не чіпаємо
    }
    const bool relayOn = (logicalInput == cfg::kInputAux);
    digitalWrite(pins::kInputRelay, levelFor(relayOn));
    s_current = logicalInput;
    Serial.printf("[SEL] input=%u relay=%s\n", static_cast<unsigned>(logicalInput), relayOn ? "on" : "off");
    return true;
}

uint8_t current() {
    return s_current;
}

}  // namespace input_selector
