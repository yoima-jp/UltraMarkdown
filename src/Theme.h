#pragma once

#include <windows.h>

enum class ThemeMode {
    Light,
    Dark,
    System,
};

struct Theme {
    COLORREF windowBackground;
    COLORREF windowBorder;
    COLORREF accentColor;
    COLORREF titleBarColor;
    COLORREF titleBarTextColor;
    COLORREF commandBarHoverBackground;
    COLORREF commandBarPressedBackground;
    COLORREF statusBarBackground;
    COLORREF statusBarBorder;
    COLORREF statusBarText;
    COLORREF statusBarMutedText;
    COLORREF editorBackground;
    COLORREF editorText;
    COLORREF editorCaret;
    COLORREF editorCaretLine;
    COLORREF editorSelectionBackground;
    COLORREF editorSelectionText;
    COLORREF previewBackground;
    COLORREF previewPaperBackground;
    COLORREF previewPaperBorder;
    COLORREF previewText;
    COLORREF previewMutedText;
    COLORREF previewQuoteBar;
    COLORREF previewRule;
    COLORREF previewCodeBackground;
    COLORREF previewCodeBorder;
    COLORREF previewCodeText;
    COLORREF previewLinkText;
    COLORREF previewScrollBarTrack;
    COLORREF previewScrollBarThumb;
    COLORREF previewScrollBarThumbHot;
    COLORREF previewScrollBarThumbActive;
    COLORREF previewSelectionBackground;
    COLORREF previewSelectionText;
    const wchar_t* uiFontName;
    const char* editorFontName;
    const wchar_t* codeFontName;
    int editorFontPoints;
    int previewBodyFontPoints;
    int previewCodeFontPoints;
    int previewHeading1FontPoints;
    int previewHeading2FontPoints;
    int previewHeading3FontPoints;
    int editorExtraLineSpacing;
};

bool IsSystemDarkModeEnabled() noexcept;
ThemeMode ResolveThemeMode(ThemeMode mode) noexcept;
bool IsDarkTheme(ThemeMode mode = ThemeMode::System) noexcept;
const Theme& GetCurrentTheme(ThemeMode mode = ThemeMode::System) noexcept;
