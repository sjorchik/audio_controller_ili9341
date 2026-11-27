#!/usr/bin/env python3
"""Генератор шрифту часу (GFXfont) для основного екрана 320x240 (FontSize::Clock).

Символи: '-', '0'..'9', ':' (коди 0x2D, 0x30..0x3A). Нічого більше: температура й
число гучності малюються FontSize::XLarge, а не цим шрифтом.
Цифри DejaVu Sans табличні; xAdvance цифр і '-' примусово вирівнюється до ширини
цифри, щоб "12:34" і заглушка "--:--" мали однакову ширину й час не «стрибав».
Двокрапка зберігає власну (вужчу) ширину; двокрапка й дефіс підняті на центр цифр по висоті. Використовує build() і emit() із
tools/gen_gfxfont.py; наявні генератори й дані НЕ чіпаються.

Використання (з кореня проєкту, одним рядком):
    python3 tools/gen_clock_font.py --ttf tools/fonts/DejaVuSans.ttf --out src/ui/font_clock_data.h --em 47

Значення --em за замовчуванням у скрипті — 47 (проєкт використовує саме його;
yAdvance звіряє static_assert у ui/fonts.cpp з display_cfg::kFontClockPx), але
вказуйте --em явно. Обмеження за макетом: висота цифри <= 36 px (em 47 дає 34..36),
ширина "12:34" <= ~150 px.
"""
import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_gfxfont as g  # noqa: E402

DIGITS = list(range(0x30, 0x3A))
COLON = 0x3A
DASH = 0x2D
CODES = [DASH] + DIGITS + [COLON]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ttf", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--em", type=int, default=47)
    args = ap.parse_args()

    g.RANGES = [(c, c) for c in CODES]
    f = g.build(args.ttf, "Clock", args.em)

    first = f["first"]
    digit_adv = f["glyphs"][0x30 - first][3]
    glyphs = []
    for i, (off, w, h, adv, xo, yo) in enumerate(f["glyphs"]):
        code = first + i
        if code in DIGITS or code == DASH:
            # табличні цифри й '-' в комірці ширини цифри (вузькі гліфи центруємо)
            xo = max(0, (digit_adv - w) // 2) if w else 0
            adv = digit_adv
        glyphs.append((off, w, h, adv, xo, yo))

    # Двокрапка DejaVu стоїть низько (від бази до висоти малих літер) — для годинника
    # центруємо її по висоті цифр: зсув yOffset на різницю центрів (лише yOffset).
    zero = glyphs[0x30 - first]
    digit_center = zero[5] + zero[2] / 2.0
    shifts = {}
    for code in (COLON, DASH):  # '-' теж на осі цифр: заглушка "--:--" вирівняна з двокрапкою
        ci = code - first
        c_off, c_w, c_h, c_adv, c_xo, c_yo = glyphs[ci]
        shift = int(round(digit_center - (c_yo + c_h / 2.0)))
        assert -128 <= c_yo + shift < 128
        glyphs[ci] = (c_off, c_w, c_h, c_adv, c_xo, c_yo + shift)
        shifts[code] = shift
    colon_shift = shifts[COLON]
    ci = COLON - first
    f["glyphs"] = glyphs

    g.emit([f], args.ttf.replace("\\", "/").split("/")[-1], args.out)
    # Шапка: emit() пише ім'я базового генератора — уточнюємо.
    txt = Path(args.out).read_text(encoding="utf-8")
    assert txt.count("ЗГЕНЕРОВАНО tools/gen_gfxfont.py") == 1
    txt = txt.replace("ЗГЕНЕРОВАНО tools/gen_gfxfont.py", "ЗГЕНЕРОВАНО tools/gen_clock_font.py", 1)
    Path(args.out).write_text(txt, encoding="utf-8")

    # --- Метрики (реальні, з таблиці, що йде в прошивку) ---
    def gl(code):
        return glyphs[code - first]

    asc, _ = g.ImageFont.truetype(args.ttf, args.em).getmetrics()
    digit_h_min = min(gl(c)[2] for c in DIGITS)
    digit_h_max = max(gl(c)[2] for c in DIGITS)
    top_min = min(asc + gl(c)[5] for c in DIGITS)  # від верху рядка до верху цифри
    top_max = max(asc + gl(c)[5] for c in DIGITS)
    colon = gl(COLON)

    def adv_width(s):
        return sum(gl(ord(ch))[3] for ch in s)

    def ink_width(s):
        # від лівого краю першого гліфа до правого краю останнього
        x = 0
        left = None
        right = 0
        for ch in s:
            _, w, _, adv, xo, _ = gl(ord(ch))
            if w:
                if left is None:
                    left = x + xo
                right = x + xo + w
            x += adv
        return right - (left or 0)

    print("Clock: em=%d yAdvance=%d ascent=%d bitmap=%d B glyph-table=%d entries (~%d B)" %
          (args.em, f["y_advance"], asc, len(f["bitmap"]), len(glyphs), len(glyphs) * 8))
    print("  colon raised by %d px, dash by %d px to centre on digits (yOffset %d / %d)" %
          (-shifts[COLON], -shifts[DASH], glyphs[COLON - first][5], glyphs[DASH - first][5]))
    print("  digit cell width=%d  colon xAdvance=%d (glyph w=%d)  dash xAdvance=%d" %
          (digit_adv, colon[3], colon[1], gl(DASH)[3]))
    print("  digit height=%d..%d  top inset (row top -> digit top)=%d..%d  below baseline=%d" %
          (digit_h_min, digit_h_max, top_min, top_max, f["y_advance"] - asc))
    for s in ("12:34", "--:--", "00:00", "88:88"):
        print("  width '%s': advance=%d ink=%d" % (s, adv_width(s), ink_width(s)))


if __name__ == "__main__":
    sys.exit(main())
