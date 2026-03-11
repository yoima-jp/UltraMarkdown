#include "EditorHost.h"

#include "LocalizedStrings.h"

#include "Theme.h"

#include <algorithm>
#include <uxtheme.h>
#include <windowsx.h>

namespace {
constexpr int kCaretWidth = 2;
constexpr UINT_PTR kEditorSubclassId = 1;

enum class ContextCommand : UINT {
    Undo = 1,
    Redo,
    Cut,
    Copy,
    Paste,
    DeleteText,
    SelectAll,
};

int GetDpiForWindowSafe(HWND hwnd) noexcept {
    return hwnd != nullptr ? static_cast<int>(::GetDpiForWindow(hwnd)) : 96;
}

POINT ResolveContextMenuPoint(HWND hwnd, POINT point) noexcept {
    if (point.x != -1 || point.y != -1) {
        return point;
    }

    RECT rect{};
    GetClientRect(hwnd, &rect);
    POINT fallback{
        rect.left + ((rect.right - rect.left) / 2),
        rect.top + ((rect.bottom - rect.top) / 2),
    };
    ClientToScreen(hwnd, &fallback);
    return fallback;
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

    SetWindowSubclass(hwnd_, SubclassProc, kEditorSubclassId, reinterpret_cast<DWORD_PTR>(this));
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
    SendEditor(SCI_SETTECHNOLOGY, SC_TECHNOLOGY_DIRECTWRITE, 0);
    SendEditor(SCI_SETFONTQUALITY, SC_EFF_QUALITY_LCD_OPTIMIZED, 0);
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
    SendEditor(SCI_USEPOPUP, SC_POPUP_NEVER, 0);
    SetWindowTheme(hwnd_, IsDarkTheme() ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    RedrawWindow(hwnd_, nullptr, nullptr, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
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

EditorCaretStatus EditorHost::GetCaretStatus() const {
    if (hwnd_ == nullptr) {
        return {};
    }

    const sptr_t currentPos = SendEditor(SCI_GETCURRENTPOS, 0, 0);
    const sptr_t lineIndex = SendEditor(SCI_LINEFROMPOSITION, 0, currentPos);
    const sptr_t lineStart = SendEditor(SCI_POSITIONFROMLINE, static_cast<uptr_t>(lineIndex), 0);

    EditorCaretStatus status;
    status.line = static_cast<int>(lineIndex) + 1;
    status.column = static_cast<int>(SendEditor(
        SCI_COUNTCHARACTERS,
        static_cast<uptr_t>(lineStart),
        currentPos)) + 1;
    return status;
}

EditorDocumentMetrics EditorHost::GetDocumentMetrics() const {
    if (hwnd_ == nullptr) {
        return {};
    }

    const sptr_t length = SendEditor(SCI_GETLENGTH, 0, 0);

    EditorDocumentMetrics metrics;
    metrics.lineCount = std::max(1, static_cast<int>(SendEditor(SCI_GETLINECOUNT, 0, 0)));
    metrics.characterCount = static_cast<int>(SendEditor(SCI_COUNTCHARACTERS, 0, length));
    return metrics;
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

void EditorHost::ShowContextMenu(POINT screenPoint) {
    if (hwnd_ == nullptr) {
        return;
    }

    HMENU menu = CreatePopupMenu();
    if (menu == nullptr) {
        return;
    }

    AppendMenuW(menu, MF_STRING | (CanUndo() ? 0 : MF_GRAYED),
                static_cast<UINT>(ContextCommand::Undo), Localize(UiString::ContextUndo));
    AppendMenuW(menu, MF_STRING | (CanRedo() ? 0 : MF_GRAYED),
                static_cast<UINT>(ContextCommand::Redo), Localize(UiString::ContextRedo));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | (HasSelection() ? 0 : MF_GRAYED),
                static_cast<UINT>(ContextCommand::Cut), Localize(UiString::ContextCut));
    AppendMenuW(menu, MF_STRING | (HasSelection() ? 0 : MF_GRAYED),
                static_cast<UINT>(ContextCommand::Copy), Localize(UiString::ContextCopy));
    AppendMenuW(menu, MF_STRING | (CanPaste() ? 0 : MF_GRAYED),
                static_cast<UINT>(ContextCommand::Paste), Localize(UiString::ContextPaste));
    AppendMenuW(menu, MF_STRING | (HasSelection() ? 0 : MF_GRAYED),
                static_cast<UINT>(ContextCommand::DeleteText), Localize(UiString::ContextDelete));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, static_cast<UINT>(ContextCommand::SelectAll),
                Localize(UiString::ContextSelectAll));

    const POINT point = ResolveContextMenuPoint(hwnd_, screenPoint);
    const UINT command = TrackPopupMenuEx(menu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD,
                                          point.x, point.y, hwnd_, nullptr);
    DestroyMenu(menu);

    switch (static_cast<ContextCommand>(command)) {
    case ContextCommand::Undo:
        Undo();
        break;
    case ContextCommand::Redo:
        Redo();
        break;
    case ContextCommand::Cut:
        Cut();
        break;
    case ContextCommand::Copy:
        Copy();
        break;
    case ContextCommand::Paste:
        Paste();
        break;
    case ContextCommand::DeleteText:
        DeleteSelection();
        break;
    case ContextCommand::SelectAll:
        SelectAll();
        break;
    default:
        break;
    }
}

bool EditorHost::IsNotificationFrom(const NMHDR* header) const noexcept {
    return header != nullptr && header->hwndFrom == hwnd_;
}

LRESULT CALLBACK EditorHost::SubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam,
                                          UINT_PTR subclassId, DWORD_PTR referenceData) {
    auto* self = reinterpret_cast<EditorHost*>(referenceData);
    if (self == nullptr) {
        return DefSubclassProc(hwnd, message, wParam, lParam);
    }

    switch (message) {
    case WM_CONTEXTMENU:
        self->ShowContextMenu({GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
        return 0;
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, SubclassProc, subclassId);
        break;
    default:
        break;
    }

    return DefSubclassProc(hwnd, message, wParam, lParam);
}

bool EditorHost::HasSelection() const {
    return SendEditor(SCI_GETSELECTIONEMPTY, 0, 0) == 0;
}

bool EditorHost::CanUndo() const {
    return SendEditor(SCI_CANUNDO, 0, 0) != 0;
}

bool EditorHost::CanRedo() const {
    return SendEditor(SCI_CANREDO, 0, 0) != 0;
}

bool EditorHost::CanPaste() const {
    return SendEditor(SCI_CANPASTE, 0, 0) != 0;
}

void EditorHost::Undo() {
    SendEditor(SCI_UNDO, 0, 0);
}

void EditorHost::Redo() {
    SendEditor(SCI_REDO, 0, 0);
}

void EditorHost::Cut() {
    SendEditor(SCI_CUT, 0, 0);
}

void EditorHost::Copy() {
    SendEditor(SCI_COPY, 0, 0);
}

void EditorHost::Paste() {
    SendEditor(SCI_PASTE, 0, 0);
}

void EditorHost::DeleteSelection() {
    SendEditor(SCI_CLEAR, 0, 0);
}

void EditorHost::SelectAll() {
    SendEditor(SCI_SELECTALL, 0, 0);
}

sptr_t EditorHost::SendEditor(UINT message, uptr_t wParam, sptr_t lParam) const {
    return ::SendMessage(hwnd_, message, static_cast<WPARAM>(wParam), static_cast<LPARAM>(lParam));
}
