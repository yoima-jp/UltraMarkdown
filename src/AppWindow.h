#pragma once

#include "Document.h"
#include "EditorHost.h"
#include "PreviewView.h"
#include "Theme.h"

#include <string>

#include <windows.h>

class AppWindow {
public:
    AppWindow(HINSTANCE instance, std::wstring startupPath);
    int Run(int showCommand);

private:
    enum class ViewMode {
        Raw,
        Preview,
    };

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK CommandBarProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK StatusBarProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleCommandBarMessage(UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleStatusBarMessage(UINT message, WPARAM wParam, LPARAM lParam);

    bool CreateMainWindow(int showCommand);
    void CreateMenus();
    void CreateAccelerators();
    bool CreateCommandBar();
    bool CreateStatusBar();
    void ResizeChildren();

    const Theme& GetTheme() const noexcept;
    void ApplyTheme();
    void ApplyWindowTheme();
    void RecreateCommandBarFont();
    void RecreateStatusBarFont();
    void UpdateTitle();
    void UpdateMenuState();
    void UpdateStatusBar();
    void PaintCommandBar(HDC hdc);
    void PaintStatusBar(HDC hdc);
    int HitTestCommandBar(POINT point) const noexcept;
    int MeasureButtonWidth(HWND hwnd, HFONT font, const wchar_t* text, int extraPadding) const noexcept;
    RECT GetCommandButtonRect(int commandId) const noexcept;
    void ShowFileMenu();
    void SetDirty(bool dirty);
    void SyncDocumentFromEditor();
    void SyncPreviewFromDocument();
    void SetViewMode(ViewMode mode);
    void ToggleDarkMode();
    bool ConfirmDiscardChanges();

    bool DoNewDocument();
    bool DoOpenDocument(const std::wstring& path = {});
    bool DoSaveDocument(bool saveAs);
    void DoSetDefaultMarkdownApp();
    void ShowError(const std::wstring& message);
    std::wstring PromptForOpenPath();
    std::wstring PromptForSavePath();

    HWND hwnd_ = nullptr;
    HWND commandBar_ = nullptr;
    HWND statusBar_ = nullptr;
    HINSTANCE instance_ = nullptr;
    HACCEL accelerators_ = nullptr;
    HMENU fileMenu_ = nullptr;
    HFONT commandBarFont_ = nullptr;
    HFONT statusFont_ = nullptr;
    Document document_;
    EditorHost editor_;
    PreviewView preview_;
    ViewMode viewMode_ = ViewMode::Raw;
    ThemeMode themeMode_ = ThemeMode::System;
    int hotCommandId_ = 0;
    int pressedCommandId_ = 0;
    RECT fileButtonRect_{};
    RECT rawButtonRect_{};
    RECT previewButtonRect_{};
    RECT darkButtonRect_{};
    std::wstring statusPrimary_;
    std::wstring statusSecondary_;
    std::wstring statusTertiary_;
    std::wstring startupPath_;
};
