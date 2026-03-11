#include "EditorHost.h"

#include "Theme.h"

#include <algorithm>

namespace {
constexpr int kCaretWidth = 2;

int GetDpiForWindowSafe(HWND hwnd) noexcept {
    return hwnd != nullptr ? static_cast<int>(::GetDpiForWindow(hwnd)) : 96;
}
}

bool EditorHost::Create(HWND parent, HINSTANCE instance, int controlId) {
    hwnd_ = CreateWindowExW(0, L"Scintilla", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                                       WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                            0, 0, 0, 0, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
                            instance, nullptr);
    if (!hwnd_) {
        return false;
    }

    ApplyTheme(GetCurrentTheme());
    return true;
}

void EditorHost::Resize(const RECT& bounds) {
    MoveWindow(hwnd_, bounds.left, bounds.top, bounds.right - bounds.left,
               bounds.bottom - bounds.top, TRUE);
}

void EditorHost::Show(bool visible) {
    ShowWindow(hwnd_, visible ? SW_SHOW : SW_HIDE);
}

void EditorHost::Focus() {
    SetFocus(hwnd_);
}

void EditorHost::ApplyTheme(const Theme& theme) {
    if (hwnd_ == nullptr) {
        return;
    }

    const int dpi = GetDpiForWindowSafe(hwnd_);
    const int extraSpacing = MulDiv(theme.editorExtraLineSpacing, dpi, 96);

    SendEditor(SCI_SETCODEPAGE, SC_CP_UTF8, 0);
    SendEditor(SCI_STYLESETFONT, STYLE_DEFAULT, reinterpret_cast<sptr_t>(theme.editorFontName));
    SendEditor(SCI_STYLESETSIZEFRACTIONAL, STYLE_DEFAULT, theme.editorFontPoints * 100);
    SendEditor(SCI_STYLESETFORE, STYLE_DEFAULT, theme.editorText);
    SendEditor(SCI_STYLESETBACK, STYLE_DEFAULT, theme.editorBackground);
    SendEditor(SCI_STYLECLEARALL, 0, 0);
    SendEditor(SCI_SETWRAPMODE, SC_WRAP_WORD, 0);
    SendEditor(SCI_SETMARGINWIDTHN, 0, 0);
    SendEditor(SCI_SETMARGINWIDTHN, 1, 0);
    SendEditor(SCI_SETMARGINWIDTHN, 2, 0);
    SendEditor(SCI_SETUSETABS, 0, 0);
    SendEditor(SCI_SETTABWIDTH, 4, 0);
    SendEditor(SCI_SETINDENT, 4, 0);
    SendEditor(SCI_SETEOLMODE, SC_EOL_CRLF, 0);
    SendEditor(SCI_SETBUFFEREDDRAW, 1, 0);
    SendEditor(SCI_SETCARETPERIOD, 0, 0);
    SendEditor(SCI_SETCARETWIDTH, kCaretWidth, 0);
    SendEditor(SCI_SETCARETFORE, theme.editorCaret, 0);
    SendEditor(SCI_SETCARETLINEVISIBLE, 1, 0);
    SendEditor(SCI_SETCARETLINEBACK, theme.editorCaretLine, 0);
    SendEditor(SCI_SETSELFORE, 1, theme.editorSelectionText);
    SendEditor(SCI_SETSELBACK, 1, theme.editorSelectionBackground);
    SendEditor(SCI_SETEXTRAASCENT, extraSpacing, 0);
    SendEditor(SCI_SETEXTRADESCENT, extraSpacing, 0);
    SendEditor(SCI_SETREADONLY, 0, 0);
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void EditorHost::SetTextUtf8(const std::string& textUtf8) {
    loading_ = true;
    SendEditor(SCI_SETREADONLY, 0, 0);
    SendEditor(SCI_SETTEXT, 0, reinterpret_cast<sptr_t>(textUtf8.c_str()));
    SendEditor(SCI_EMPTYUNDOBUFFER, 0, 0);
    SendEditor(SCI_SETSAVEPOINT, 0, 0);
    SendEditor(SCI_GOTOPOS, 0, 0);
    loading_ = false;
}

std::string EditorHost::GetTextUtf8() const {
    const auto length = static_cast<size_t>(SendEditor(SCI_GETTEXTLENGTH, 0, 0));
    std::string buffer(length + 1, '\0');
    SendEditor(SCI_GETTEXT, static_cast<uptr_t>(buffer.size()), reinterpret_cast<sptr_t>(buffer.data()));
    buffer.resize(length);
    return buffer;
}

EditorStatus EditorHost::GetStatus() const {
    if (hwnd_ == nullptr) {
        return {};
    }

    const sptr_t length = SendEditor(SCI_GETLENGTH, 0, 0);
    const sptr_t currentPos = SendEditor(SCI_GETCURRENTPOS, 0, 0);
    const sptr_t lineIndex = SendEditor(SCI_LINEFROMPOSITION, 0, currentPos);
    const sptr_t lineStart = SendEditor(SCI_POSITIONFROMLINE, static_cast<uptr_t>(lineIndex), 0);

    EditorStatus status;
    status.line = static_cast<int>(lineIndex) + 1;
    status.column = static_cast<int>(SendEditor(
        SCI_COUNTCHARACTERS,
        static_cast<uptr_t>(lineStart),
        currentPos)) + 1;
    status.lineCount = std::max(1, static_cast<int>(SendEditor(SCI_GETLINECOUNT, 0, 0)));
    status.characterCount = static_cast<int>(SendEditor(SCI_COUNTCHARACTERS, 0, length));
    return status;
}

void EditorHost::MarkClean() {
    SendEditor(SCI_SETSAVEPOINT, 0, 0);
}

bool EditorHost::IsDirty() const {
    return SendEditor(SCI_GETMODIFY, 0, 0) != 0;
}

void EditorHost::SetReadOnly(bool readOnly) {
    SendEditor(SCI_SETREADONLY, readOnly ? 1 : 0, 0);
}

bool EditorHost::IsNotificationFrom(const NMHDR* header) const noexcept {
    return header != nullptr && header->hwndFrom == hwnd_;
}

sptr_t EditorHost::SendEditor(UINT message, uptr_t wParam, sptr_t lParam) const {
    return ::SendMessage(hwnd_, message, static_cast<WPARAM>(wParam), static_cast<LPARAM>(lParam));
}
