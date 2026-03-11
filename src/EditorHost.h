#pragma once

#include "Theme.h"

#include <string>

#include <commctrl.h>
#include <windows.h>
#include <Scintilla.h>

struct EditorCaretStatus {
    int line = 1;
    int column = 1;
};

struct EditorDocumentMetrics {
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
    EditorCaretStatus GetCaretStatus() const;
    EditorDocumentMetrics GetDocumentMetrics() const;

    void MarkClean();
    bool IsDirty() const;

    void SetReadOnly(bool readOnly);
    void ShowContextMenu(POINT screenPoint);

    HWND GetHandle() const noexcept { return hwnd_; }
    bool IsNotificationFrom(const NMHDR* header) const noexcept;
    bool IsLoading() const noexcept { return loading_; }

private:
    static LRESULT CALLBACK SubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam,
                                         UINT_PTR subclassId, DWORD_PTR referenceData);
    sptr_t SendEditor(UINT message, uptr_t wParam = 0, sptr_t lParam = 0) const;
    bool HasSelection() const;
    bool CanUndo() const;
    bool CanRedo() const;
    bool CanPaste() const;
    void Undo();
    void Redo();
    void Cut();
    void Copy();
    void Paste();
    void DeleteSelection();
    void SelectAll();

    HWND hwnd_ = nullptr;
    bool loading_ = false;
};
