#include "EditorHost.h"

namespace {
constexpr COLORREF kBackColor = RGB(250, 251, 253);
constexpr COLORREF kTextColor = RGB(39, 45, 54);
constexpr COLORREF kCaretColor = RGB(31, 111, 235);
constexpr COLORREF kCaretLine = RGB(243, 247, 252);
constexpr COLORREF kSelectionBack = RGB(209, 228, 252);
constexpr COLORREF kSelectionFore = RGB(22, 46, 74);
}

bool EditorHost::Create(HWND parent, HINSTANCE instance, int controlId) {
    hwnd_ = CreateWindowExW(0, L"Scintilla", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                                       WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                            0, 0, 0, 0, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
                            instance, nullptr);
    if (!hwnd_) {
        return false;
    }

    ConfigureDefaults();
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

void EditorHost::ConfigureDefaults() {
    SendEditor(SCI_SETCODEPAGE, SC_CP_UTF8, 0);
    SendEditor(SCI_STYLESETFONT, STYLE_DEFAULT, reinterpret_cast<sptr_t>("Consolas"));
    SendEditor(SCI_STYLESETSIZE, STYLE_DEFAULT, 13);
    SendEditor(SCI_STYLESETFORE, STYLE_DEFAULT, kTextColor);
    SendEditor(SCI_STYLESETBACK, STYLE_DEFAULT, kBackColor);
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
    SendEditor(SCI_SETCARETFORE, kCaretColor, 0);
    SendEditor(SCI_SETCARETLINEVISIBLE, 1, 0);
    SendEditor(SCI_SETCARETLINEBACK, kCaretLine, 0);
    SendEditor(SCI_SETSELFORE, 1, kSelectionFore);
    SendEditor(SCI_SETSELBACK, 1, kSelectionBack);
    SendEditor(SCI_SETEXTRAASCENT, 2, 0);
    SendEditor(SCI_SETEXTRADESCENT, 2, 0);
    SendEditor(SCI_SETREADONLY, 0, 0);
}

sptr_t EditorHost::SendEditor(UINT message, uptr_t wParam, sptr_t lParam) const {
    return ::SendMessage(hwnd_, message, static_cast<WPARAM>(wParam), static_cast<LPARAM>(lParam));
}
