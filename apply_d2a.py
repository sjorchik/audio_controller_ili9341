#!/usr/bin/env python3
"""D2a: правки великих файлів (4 входи). Запуск з кореня репозиторію:
    python apply_d2a.py [корінь_репозиторію]
Кожна правка: assert text.count(old) == 1 (інакше скрипт зупиняється й нічого не пише в цей файл).
Закінчення рядків (CRLF / LF) файлу зберігаються. Повторний запуск безпечний: після першого застосування
old більше не знаходиться й скрипт зупиниться на assert (файл не пошкоджується)."""
import sys, os

PATCHES = {}

# ---------------------------------------------------------------- src/config/screens_config.h
PATCHES['src/config/screens_config.h'] = [
('''// [Prompt 48a] Світлі кольори назви за номером зовнішнього входу 1..2 (індекс масиву = inputIndex - 1):
// TV Box і Computer. Підібрано, щоб відрізнятись від бірюзового VU (kColorVuLit = 0,235,215): зелений —
// без синьої складової. Кольори колишніх входів 3 / 4 (блакитний, фіолетовий) прибрано разом із входами.
constexpr uint16_t kColorInputName[] = {
    display_cfg::rgb565(120, 255, 120),   // вхід 1 (TV Box) — світло-зелений
    display_cfg::rgb565(255, 120, 120),   // вхід 2 (Computer) — світло-червоний
};''',
'''// [D2a] Світлі кольори назви за номером не-радіо входу 1..3 (індекс масиву = inputIndex - 1):
// Computer, Aux, Плеєр. Підібрано, щоб відрізнятись від бірюзового VU (kColorVuLit = 0,235,215): червоний і
// зелений — без синьої складової; Плеєр — світло-фіолетовий (синя складова є, але з сильною червоною — на тлі
// бірюзового VU не плутається).
constexpr uint16_t kColorInputName[] = {
    display_cfg::rgb565(255, 120, 120),   // вхід 1 (Computer) — світло-червоний
    display_cfg::rgb565(120, 255, 120),   // вхід 2 (Aux) — світло-зелений
    display_cfg::rgb565(200, 150, 255),   // вхід 3 (Плеєр) — світло-фіолетовий
};'''),
]

# ---------------------------------------------------------------- src/core/app_controller.cpp
PATCHES['src/core/app_controller.cpp'] = [
# 1. include
('''#include "net/wifi_manager.h"       // [Prompt 12] ДОДАНО
''','''#include "net/wifi_manager.h"       // [Prompt 12] ДОДАНО
#include "config/inputs_config.h"    // [D2a] inputs_cfg::usesLinePath(): 1 = Computer, 2 = Aux через line_path; 3 = Плеєр без проходу
'''),
# 2. lineApplyEq: лише входи з проходом
('''    if (idx == 0 || idx >= defaults::kInputCount || !LinePath::isReady()) {
        return;
    }
    const Settings st = SettingsStore::snapshot();
    LinePath::setEqAllDb(st.profiles[idx].eqGainsDb);''','''    if (!inputs_cfg::usesLinePath(idx) || !LinePath::isReady()) {  // [D2a] радіо (0) і Плеєр (3) — не прохід
        return;
    }
    const Settings st = SettingsStore::snapshot();
    LinePath::setEqAllDb(st.profiles[idx].eqGainsDb);'''),
# 3. lineReleaseDone: рішення «що далі»
('''    if (s_input != 0) {
        if (after == LineAfter::Fail) {''','''    if (inputs_cfg::usesLinePath(s_input)) {  // [D2a] було s_input != 0: Плеєр (3) проходу не потребує
        if (after == LineAfter::Fail) {'''),
('''    // Вхід — радіо: RAM проходу вже звільнена, стартуємо потік. Settle перезапускаємо: радіо до розмʼюту в мʼюті.
    startTransition();''','''    if (s_input != 0) {
        return;  // [D2a] Плеєр (3): ресурси проходу звільнено, звук мовчить (радіо не стартуємо)
    }
    // Вхід — радіо: RAM проходу вже звільнена, стартуємо потік. Settle перезапускаємо: радіо до розмʼюту в мʼюті.
    startTransition();'''),
# 4. routeInput: Плеєр
('''        return;
    }
    switch (s_lineState) {
        case LineState::Off:
        case LineState::Failed:
            lineBegin();
            break;''','''        return;
    }
    if (!inputs_cfg::usesLinePath(s_input)) {
        // [D2a] Плеєр (3): заглушка. Прохід не запускаємо, радіо не стартуємо, реле не чіпаємо: вихід у тиші.
        // Якщо прохід був активним (Computer / Aux -> Плеєр) — звільняємо його; lineReleaseDone() далі нічого не стартує.
        switch (s_lineState) {
            case LineState::Off:
            case LineState::Failed:
                lineSetState(LineState::Off);
                break;
            case LineState::Releasing:
                s_lineAfter = LineAfter::Auto;
                break;
            default:
                s_lineAfter = LineAfter::Auto;
                lineBeginRelease();  // мʼют -> returnToRadio() -> тиша (lineReleaseDone)
                break;
        }
        return;
    }
    switch (s_lineState) {
        case LineState::Off:
        case LineState::Failed:
            lineBegin();
            break;'''),
# 5. lineSkipsSettle (LINE_PATH_ENABLE == 1)
('''inline bool lineSkipsSettle(uint8_t prev, uint8_t next) {
    return prev != 0 && next != 0;
}''','''inline bool lineSkipsSettle(uint8_t prev, uint8_t next) {
    return inputs_cfg::usesLinePath(prev) && inputs_cfg::usesLinePath(next);  // [D2a] було prev != 0 && next != 0
}'''),
# 6. IR: екран «EQ + баланс» (INPUT_PC -> 1, INPUT_TV без дії)
('''        case Action::INPUT_RADIO: if (plain) selectInput(0); break;
        case Action::INPUT_TV:    if (plain) selectInput(1); break;
        case Action::INPUT_PC:    if (plain) selectInput(2); break;
        default:
            break;  // решта дій у екрані ігнорується''','''        case Action::INPUT_RADIO: if (plain) selectInput(inputs_cfg::kRadio); break;
        case Action::INPUT_TV:    break;  // [D2a] входу TV Box немає: дія лишається в enum (мапа IR), без дії
        case Action::INPUT_PC:    if (plain) selectInput(inputs_cfg::kComputer); break;
        default:
            break;  // решта дій у екрані ігнорується'''),
# 7. IR: головний обробник
('''        case Action::INPUT_RADIO: if (plain) selectInput(0); break;
        case Action::INPUT_TV:    if (plain) selectInput(1); break;
        case Action::INPUT_PC:    if (plain) selectInput(2); break;
        case Action::INPUT_AUX:   // [P48a] входу AUX немає: дія лишається в enum (мапа IR), без дії
            if (plain) APP_LOG("INPUT_AUX ignored (no such input)\\n");
            break;''','''        case Action::INPUT_RADIO: if (plain) selectInput(inputs_cfg::kRadio); break;
        case Action::INPUT_TV:    // [D2a] входу TV Box немає: дія лишається в enum (мапа IR), без дії
            if (plain) APP_LOG("INPUT_TV ignored (no such input)\\n");
            break;
        case Action::INPUT_PC:    if (plain) selectInput(inputs_cfg::kComputer); break;
        case Action::INPUT_AUX:   // [D2a] Aux з пульта поки недоступний (дія не навчається): без дії
            if (plain) APP_LOG("INPUT_AUX ignored (not mapped on remote)\\n");
            break;'''),
# 8. applyLineEq
('''    if (input != 0 && input == s_input && LinePath::isReady()) {''','''    if (inputs_cfg::usesLinePath(input) && input == s_input && LinePath::isReady()) {  // [D2a] Плеєр (3) — не прохід'''),
# 9. коментарі
('''    const int count = defaults::kInputCount;  // [P48a] 3 логічні входи; вимкнені маскою пропускаються''','''    const int count = defaults::kInputCount;  // [D2a] 4 логічні входи (радіо, Computer, Aux, Плеєр); вимкнені маскою пропускаються'''),
('''                why = "input_unavailable";  // поза 0..2''','''                why = "input_unavailable";  // поза 0..kInputCount-1 (0..3)'''),
('''        // [Prompt 48a] Реле не чіпаємо (лишається в TV Box після input_selector::begin()), тиша: профіль''','''        // [Prompt 48a / D2a] Реле не чіпаємо (лишається в Computer після input_selector::begin()), тиша: профіль'''),
]

def main():
    root = sys.argv[1] if len(sys.argv) > 1 else '.'
    for rel, patches in PATCHES.items():
        path = os.path.join(root, *rel.split('/'))
        with open(path, 'r', encoding='utf-8', newline='') as f:
            raw = f.read()
        crlf = raw.count('\r\n') > raw.count('\n') / 2
        text = raw.replace('\r\n', '\n')
        for i, (old, new) in enumerate(patches, 1):
            n = text.count(old)
            assert n == 1, '%s: patch #%d matches %d times (need exactly 1): %r' % (rel, i, n, old[:80])
            text = text.replace(old, new)
        if crlf:
            text = text.replace('\n', '\r\n')
        with open(path, 'w', encoding='utf-8', newline='') as f:
            f.write(text)
        print('patched %s (%d edits, %s)' % (rel, len(patches), 'CRLF' if crlf else 'LF'))

if __name__ == '__main__':
    main()
