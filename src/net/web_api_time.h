#pragma once

// [S2a] GET /api/time — поточний час пристрою (див. core/time_service.h).
// Відповідь 200: {"valid":bool,"epoch":N|null,"local":"HH:MM:SS"|null,"date":"YYYY-MM-DD"|null,
//                 "tz":"EET/EEST","msSinceSync":N|null}.
// Поки час недійсний — epoch / local / date / msSinceSync = null.

class AsyncWebServer;

// Реєструє GET /api/time. Викликається з registerRoutes() у net/web_server.cpp.
void registerTimeRoutes(AsyncWebServer& server);
