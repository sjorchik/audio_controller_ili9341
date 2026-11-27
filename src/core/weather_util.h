#pragma once

// [S2b] Чисті допоміжні функції сервісу погоди (без Arduino / FreeRTOS): обрізання й перевірка UTF-8,
// URL-кодування, округлення. Винесено в окремий заголовок, щоб перевірити на ПК (test_host_s2b).

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace weather_util {

// Копіює UTF-8 рядок у dst (cap байт разом із NUL), не розрізаючи багатобайтний символ.
inline void copyUtf8Trunc(char* dst, size_t cap, const char* src) {
    if (dst == nullptr || cap == 0) return;
    if (src == nullptr) {
        dst[0] = '\0';
        return;
    }
    size_t n = strlen(src);
    if (n > cap - 1) {
        n = cap - 1;
        // src[n] — перший НЕскопійований байт; якщо це продовження (10xxxxxx), розріз у середині
        // символу: відступаємо до початкового байта символу, який теж не копіюємо.
        while (n > 0 && (static_cast<uint8_t>(src[n]) & 0xC0u) == 0x80u) --n;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
}

// Строга перевірка коректності UTF-8 (без надкоротких форм, сурогатів і кодових точок > U+10FFFF).
inline bool isValidUtf8(const char* s, size_t len) {
    size_t i = 0;
    while (i < len) {
        const uint8_t c = static_cast<uint8_t>(s[i]);
        if (c < 0x80) {
            ++i;
        } else if (c >= 0xC2 && c <= 0xDF) {
            if (i + 1 >= len || (static_cast<uint8_t>(s[i + 1]) & 0xC0u) != 0x80u) return false;
            i += 2;
        } else if (c >= 0xE0 && c <= 0xEF) {
            if (i + 2 >= len) return false;
            const uint8_t c1 = static_cast<uint8_t>(s[i + 1]);
            const uint8_t c2 = static_cast<uint8_t>(s[i + 2]);
            if ((c1 & 0xC0u) != 0x80u || (c2 & 0xC0u) != 0x80u) return false;
            if (c == 0xE0 && c1 < 0xA0) return false;   // надкоротка форма
            if (c == 0xED && c1 > 0x9F) return false;   // сурогати U+D800..U+DFFF
            i += 3;
        } else if (c >= 0xF0 && c <= 0xF4) {
            if (i + 3 >= len) return false;
            const uint8_t c1 = static_cast<uint8_t>(s[i + 1]);
            const uint8_t c2 = static_cast<uint8_t>(s[i + 2]);
            const uint8_t c3 = static_cast<uint8_t>(s[i + 3]);
            if ((c1 & 0xC0u) != 0x80u || (c2 & 0xC0u) != 0x80u || (c3 & 0xC0u) != 0x80u) return false;
            if (c == 0xF0 && c1 < 0x90) return false;   // надкоротка форма
            if (c == 0xF4 && c1 > 0x8F) return false;   // > U+10FFFF
            i += 4;
        } else {
            return false;  // продовження без початку, C0 / C1, F5..FF
        }
    }
    return true;
}

// Текст користувача (назва міста, запит): 1..maxBytes байт, коректний UTF-8, без керувальних
// символів (U+0000..U+001F, U+007F, U+0080..U+009F).
inline bool isTextOk(const char* s, size_t maxBytes) {
    if (s == nullptr) return false;
    const size_t len = strlen(s);
    if (len == 0 || len > maxBytes) return false;
    if (!isValidUtf8(s, len)) return false;
    for (size_t i = 0; i < len; ++i) {
        const uint8_t c = static_cast<uint8_t>(s[i]);
        if (c < 0x20 || c == 0x7F) return false;
        if (c == 0xC2 && i + 1 < len) {
            const uint8_t c1 = static_cast<uint8_t>(s[i + 1]);
            if (c1 >= 0x80 && c1 <= 0x9F) return false;  // C1-керувальні
        }
    }
    return true;
}

// Початок і довжина рядка без пробілів / табуляцій по краях (на місці, без копіювання).
inline const char* trimSpan(const char* s, size_t& len) {
    if (s == nullptr) {
        len = 0;
        return "";
    }
    while (*s == ' ' || *s == '\t') ++s;
    len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t')) --len;
    return s;
}

// Percent-encoding UTF-8 (RFC 3986: без кодування лишаються A-Z a-z 0-9 - _ . ~).
// Повертає довжину результату (без NUL) або 0, якщо out замалий.
inline size_t urlEncode(const char* in, size_t inLen, char* out, size_t cap) {
    static const char kHex[] = "0123456789ABCDEF";
    size_t o = 0;
    for (size_t i = 0; i < inLen; ++i) {
        const uint8_t c = static_cast<uint8_t>(in[i]);
        const bool plain = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                           c == '-' || c == '_' || c == '.' || c == '~';
        if (plain) {
            if (o + 1 >= cap) return 0;
            out[o++] = static_cast<char>(c);
        } else {
            if (o + 3 >= cap) return 0;
            out[o++] = '%';
            out[o++] = kHex[c >> 4];
            out[o++] = kHex[c & 0x0F];
        }
    }
    if (o >= cap) return 0;
    out[o] = '\0';
    return o;
}

// Округлення до 1 / 4 знаків: результат у double, щоб JSON друкував «3.4», а не «3.4000001».
inline float round1(float v) { return roundf(v * 10.0f) / 10.0f; }
inline double round1d(float v) { return static_cast<double>(lroundf(v * 10.0f)) / 10.0; }
inline double round4d(float v) { return static_cast<double>(lroundf(v * 10000.0f)) / 10000.0; }

inline bool inRange(float v, float lo, float hi) { return isfinite(v) && v >= lo && v <= hi; }

}  // namespace weather_util
