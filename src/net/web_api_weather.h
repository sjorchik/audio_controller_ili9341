#pragma once

// [S2b] HTTP-маршрути погоди (за зразком web_api_time.h): реєструються одним викликом із web_server.cpp.
//   GET    /api/weather            — стан: місце, поточна погода, помилка
//   POST   /api/weather/search     — {"query":"..."} -> 202, пошук міста (мережа — у задачі "weather")
//   GET    /api/weather/search     — стан пошуку й кандидати
//   POST   /api/weather/location   — {"index":N} АБО {"name","lat","lon"}
//   DELETE /api/weather/location   — очистити місце
// Обробники не блокують (виконуються в задачі async_tcp): лише читають / змінюють стан у RAM.
// Контракт — docs/web_api.md.

#include <ESPAsyncWebServer.h>

void registerWeatherRoutes(AsyncWebServer& server);
