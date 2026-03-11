#pragma once

#include <string>

#include <windows.h>

struct DefaultMarkdownAppResult {
    bool success = false;
    std::wstring error;
};

DefaultMarkdownAppResult RegisterAsDefaultMarkdownApp(HWND owner);
