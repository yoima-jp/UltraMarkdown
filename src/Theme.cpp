#include "Theme.h"

#include <winreg.h>

namespace {
constexpr COLORREF MakeColor(BYTE red, BYTE green, BYTE blue) noexcept {
    return RGB(red, green, blue);
}

constexpr Theme kLightTheme{
    MakeColor(244, 247, 251),
    MakeColor(220, 226, 234),
    MakeColor(0, 120, 212),
    MakeColor(249, 250, 252),
    MakeColor(34, 40, 48),
    MakeColor(232, 236, 241),   // commandBarHoverBackground
    MakeColor(220, 225, 232),   // commandBarPressedBackground
    MakeColor(240, 244, 248),
    MakeColor(220, 226, 234),
    MakeColor(46, 54, 62),
    MakeColor(103, 113, 124),
    MakeColor(251, 252, 254),
    MakeColor(33, 40, 48),
    MakeColor(0, 120, 212),
    MakeColor(243, 247, 252),
    MakeColor(209, 228, 252),
    MakeColor(24, 48, 78),
    MakeColor(241, 244, 248),
    MakeColor(255, 255, 255),
    MakeColor(225, 231, 238),
    MakeColor(33, 40, 48),
    MakeColor(108, 117, 127),
    MakeColor(211, 219, 228),
    MakeColor(216, 223, 231),
    MakeColor(247, 249, 252),
    MakeColor(226, 231, 237),
    MakeColor(128, 53, 30),
    MakeColor(14, 98, 191),
    MakeColor(229, 234, 240),
    MakeColor(164, 175, 188),
    MakeColor(141, 153, 168),
    MakeColor(118, 131, 147),
    MakeColor(206, 228, 255),
    MakeColor(16, 54, 96),
    L"Segoe UI",
    "Consolas",
    L"Consolas",
    13,
    11,
    10,
    20,
    16,
    13,
    3,
};

constexpr Theme kDarkTheme{
    MakeColor(24, 24, 26),
    MakeColor(58, 58, 60),
    MakeColor(0, 122, 204),
    MakeColor(30, 30, 30),
    MakeColor(243, 243, 243),
    MakeColor(50, 50, 52),      // commandBarHoverBackground
    MakeColor(62, 62, 64),      // commandBarPressedBackground
    MakeColor(37, 37, 38),
    MakeColor(60, 60, 64),
    MakeColor(212, 212, 212),
    MakeColor(152, 152, 156),
    MakeColor(30, 30, 30),
    MakeColor(212, 212, 212),
    MakeColor(86, 156, 214),
    MakeColor(43, 45, 48),
    MakeColor(38, 79, 120),
    MakeColor(238, 243, 250),
    MakeColor(30, 30, 30),
    MakeColor(37, 37, 38),
    MakeColor(58, 58, 60),
    MakeColor(212, 212, 212),
    MakeColor(150, 150, 150),
    MakeColor(84, 84, 88),
    MakeColor(70, 70, 74),
    MakeColor(45, 45, 45),
    MakeColor(66, 66, 70),
    MakeColor(214, 157, 133),
    MakeColor(78, 163, 255),
    MakeColor(44, 44, 46),
    MakeColor(96, 96, 102),
    MakeColor(122, 122, 130),
    MakeColor(150, 150, 158),
    MakeColor(38, 79, 120),
    MakeColor(245, 249, 255),
    L"Segoe UI",
    "Consolas",
    L"Consolas",
    13,
    11,
    10,
    20,
    16,
    13,
    3,
};
}

bool IsSystemDarkModeEnabled() noexcept {
    DWORD value = 1;
    DWORD size = sizeof(value);
    const LSTATUS status = RegGetValueW(
        HKEY_CURRENT_USER,
        LR"(Software\Microsoft\Windows\CurrentVersion\Themes\Personalize)",
        L"AppsUseLightTheme",
        RRF_RT_REG_DWORD,
        nullptr,
        &value,
        &size);
    return status == ERROR_SUCCESS && value == 0;
}

ThemeMode ResolveThemeMode(ThemeMode mode) noexcept {
    if (mode == ThemeMode::System) {
        return IsSystemDarkModeEnabled() ? ThemeMode::Dark : ThemeMode::Light;
    }
    return mode;
}

bool IsDarkTheme(ThemeMode mode) noexcept {
    return ResolveThemeMode(mode) == ThemeMode::Dark;
}

const Theme& GetCurrentTheme(ThemeMode mode) noexcept {
    return ResolveThemeMode(mode) == ThemeMode::Dark ? kDarkTheme : kLightTheme;
}
