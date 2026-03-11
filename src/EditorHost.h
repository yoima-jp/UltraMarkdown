#pragma once

#include "Theme.h"

#include <string>

#include <windows.h>
#include <Scintilla.h>

struct EditorStatus {
    int line = 1;
    int column = 1;
    int lineCount = 1;
    int characterCount = 0;
};

class EditorHost {
public:
    bool Create(HWND parent, HINSTANCE instance, int controlId);
    void Resize(const RECT& bounds);
    void Show(bool visible);
    void Focus();
    void ApplyTheme(const Theme& theme);

    void SetTextUtf8(const std::string& textUtf8);
    std::string GetTextUtf8() const;
    EditorStatus GetStatus() const;

    void MarkClean();
    bool IsDirty() const;

    void SetReadOnly(bool readOnly);

    HWND GetHandle() const noexcept { return hwnd_; }
    bool IsNotificationFrom(const NMHDR* header) const noexcept;
    bool IsLoading() const noexcept { return loading_; }

private:
    sptr_t SendEditor(UINT message, uptr_t wParam = 0, sptr_t lParam = 0) const;

    HWND hwnd_ = nullptr;
    bool loading_ = false;
};
