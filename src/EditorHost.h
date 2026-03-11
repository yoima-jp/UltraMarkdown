#pragma once

#include <string>

#include <windows.h>
#include <Scintilla.h>

class EditorHost {
public:
    bool Create(HWND parent, HINSTANCE instance, int controlId);
    void Resize(const RECT& bounds);
    void Show(bool visible);
    void Focus();

    void SetTextUtf8(const std::string& textUtf8);
    std::string GetTextUtf8() const;

    void MarkClean();
    bool IsDirty() const;

    void SetReadOnly(bool readOnly);

    HWND GetHandle() const noexcept { return hwnd_; }
    bool IsNotificationFrom(const NMHDR* header) const noexcept;
    bool IsLoading() const noexcept { return loading_; }

private:
    void ConfigureDefaults();
    sptr_t SendEditor(UINT message, uptr_t wParam = 0, sptr_t lParam = 0) const;

    HWND hwnd_ = nullptr;
    bool loading_ = false;
};
