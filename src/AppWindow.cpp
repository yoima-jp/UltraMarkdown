#include "AppWindow.h"

#include "LocalizedStrings.h"
#include "resource_ids.h"

#include <algorithm>
#include <commdlg.h>
#include <cwchar>
#include <dwmapi.h>
#include <shellapi.h>
#include <windowsx.h>

namespace {
constexpr wchar_t kWindowClassName[] = L"UltraMarkdownMainWindow";
constexpr wchar_t kCommandBarClassName[] = L"UltraMarkdownCommandBar";
constexpr wchar_t kStatusBarClassName[] = L"UltraMarkdownStatusBar";

constexpr int kCommandFile = 1;

int GetDpiForWindowSafe(HWND hwnd) noexcept {
    return hwnd != nullptr ? static_cast<int>(GetDpiForWindow(hwnd)) : 96;
}

int ScaleForWindow(HWND hwnd, int value) noexcept {
    return MulDiv(value, GetDpiForWindowSafe(hwnd), 96);
}

HFONT CreateUiFont(HWND hwnd, const wchar_t* faceName, int pointSize, int weight = FW_NORMAL) {
    LOGFONTW font{};
    font.lfHeight = -MulDiv(pointSize, GetDpiForWindowSafe(hwnd), 72);
    font.lfWeight = weight;
    font.lfQuality = CLEARTYPE_QUALITY;
    wcsncpy_s(font.lfFaceName, faceName, _TRUNCATE);
    return CreateFontIndirectW(&font);
}

void FillSolidRect(HDC hdc, const RECT& rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
}

void FrameRectSolid(HDC hdc, const RECT& rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FrameRect(hdc, &rect, brush);
    DeleteObject(brush);
}

bool RegisterChildClass(HINSTANCE instance, const wchar_t* className, WNDPROC proc) {
    WNDCLASSW wc{};
    if (GetClassInfoW(instance, className, &wc) != 0) {
        return true;
    }
    wc.lpfnWndProc = proc;
    wc.hInstance = instance;
    wc.lpszClassName = className;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    return RegisterClassW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

RECT MakeRect(int left, int top, int right, int bottom) noexcept {
    RECT rect{left, top, right, bottom};
    return rect;
}
}

AppWindow::AppWindow(HINSTANCE instance, std::wstring startupPath)
    : instance_(instance), startupPath_(std::move(startupPath)) {
}

int AppWindow::Run(int showCommand) {
    if (!Scintilla_RegisterClasses(instance_)) {
        MessageBoxW(nullptr, Localize(UiString::ErrorScintillaInit), Localize(UiString::AppName), MB_ICONERROR | MB_OK);
        return 1;
    }
    if (!CreateMainWindow(showCommand)) {
        Scintilla_ReleaseResources();
        return 1;
    }
    if (!startupPath_.empty()) {
        std::wstring path = startupPath_;
        startupPath_.clear();
        DoOpenDocument(path);
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!TranslateAcceleratorW(hwnd_, accelerators_, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    if (commandBarFont_ != nullptr) {
        DeleteObject(commandBarFont_);
        commandBarFont_ = nullptr;
    }
    if (statusFont_ != nullptr) {
        DeleteObject(statusFont_);
        statusFont_ = nullptr;
    }
    if (accelerators_ != nullptr) {
        DestroyAcceleratorTable(accelerators_);
        accelerators_ = nullptr;
    }
    if (fileMenu_ != nullptr) {
        DestroyMenu(fileMenu_);
        fileMenu_ = nullptr;
    }

    Scintilla_ReleaseResources();
    return static_cast<int>(message.wParam);
}

bool AppWindow::CreateMainWindow(int showCommand) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = AppWindow::WindowProc;
    wc.hInstance = instance_;
    wc.lpszClassName = kWindowClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hbrBackground = nullptr;
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    hwnd_ = CreateWindowExW(0, kWindowClassName, Localize(UiString::AppName), WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                            CW_USEDEFAULT, CW_USEDEFAULT, 1100, 760, nullptr, nullptr, instance_, this);
    if (!hwnd_) {
        return false;
    }

    CreateMenus();
    CreateAccelerators();
    ApplyTheme();
    UpdateStatusBar();
    ShowWindow(hwnd_, showCommand);
    UpdateWindow(hwnd_);
    return true;
}

void AppWindow::CreateMenus() {
    fileMenu_ = CreatePopupMenu();
    AppendMenuW(fileMenu_, MF_STRING, ID_FILE_NEW, Localize(UiString::MenuNew));
    AppendMenuW(fileMenu_, MF_STRING, ID_FILE_OPEN, Localize(UiString::MenuOpen));
    AppendMenuW(fileMenu_, MF_STRING, ID_FILE_SAVE, Localize(UiString::MenuSave));
    AppendMenuW(fileMenu_, MF_STRING, ID_FILE_SAVE_AS, Localize(UiString::MenuSaveAs));
    AppendMenuW(fileMenu_, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(fileMenu_, MF_STRING, ID_FILE_EXIT, Localize(UiString::MenuExit));

    SetMenu(hwnd_, nullptr);
}

void AppWindow::CreateAccelerators() {
    ACCEL entries[] = {
        {FVIRTKEY | FCONTROL, 'N', static_cast<WORD>(ID_FILE_NEW)},
        {FVIRTKEY | FCONTROL, 'O', static_cast<WORD>(ID_FILE_OPEN)},
        {FVIRTKEY | FCONTROL, 'S', static_cast<WORD>(ID_FILE_SAVE)},
        {FVIRTKEY | FCONTROL | FSHIFT, 'S', static_cast<WORD>(ID_FILE_SAVE_AS)},
        {FVIRTKEY | FCONTROL, '1', static_cast<WORD>(ID_VIEW_RAW)},
        {FVIRTKEY | FCONTROL, '2', static_cast<WORD>(ID_VIEW_PREVIEW)},
        {FVIRTKEY | FCONTROL, 'D', static_cast<WORD>(ID_VIEW_DARKMODE)},
        {FVIRTKEY, VK_F6, static_cast<WORD>(ID_VIEW_TOGGLE_PREVIEW)},
    };
    accelerators_ = CreateAcceleratorTableW(entries, static_cast<int>(sizeof(entries) / sizeof(entries[0])));
}

bool AppWindow::CreateCommandBar() {
    if (!RegisterChildClass(instance_, kCommandBarClassName, AppWindow::CommandBarProc)) {
        return false;
    }
    commandBar_ = CreateWindowExW(0, kCommandBarClassName, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 0, 0, 0,
                                  hwnd_, nullptr, instance_, this);
    return commandBar_ != nullptr;
}

bool AppWindow::CreateStatusBar() {
    if (!RegisterChildClass(instance_, kStatusBarClassName, AppWindow::StatusBarProc)) {
        return false;
    }
    statusBar_ = CreateWindowExW(0, kStatusBarClassName, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 0, 0, 0,
                                 hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_STATUSBAR)), instance_, this);
    return statusBar_ != nullptr;
}

void AppWindow::ResizeChildren() {
    RECT client{};
    GetClientRect(hwnd_, &client);

    const int commandHeight = ScaleForWindow(hwnd_, 38);
    const int statusHeight = ScaleForWindow(hwnd_, 24);
    const int buttonInset = ScaleForWindow(hwnd_, 6);
    const int buttonHeight = commandHeight - (buttonInset * 2);
    const int buttonGap = ScaleForWindow(hwnd_, 8);

    if (commandBar_ != nullptr) {
        MoveWindow(commandBar_, client.left, client.top, client.right - client.left, commandHeight, TRUE);
    }
    if (statusBar_ != nullptr) {
        MoveWindow(statusBar_, client.left, client.bottom - statusHeight, client.right - client.left, statusHeight, TRUE);
    }

    int x = ScaleForWindow(hwnd_, 12);
    const int y = buttonInset;
    const int fileWidth = ScaleForWindow(hwnd_, 76);
    const int rawWidth = ScaleForWindow(hwnd_, 74);
    const int previewWidth = ScaleForWindow(hwnd_, 94);
    const int darkWidth = ScaleForWindow(hwnd_, 72);
    fileButtonRect_ = MakeRect(x, y, x + fileWidth, y + buttonHeight);
    x = fileButtonRect_.right + buttonGap;
    rawButtonRect_ = MakeRect(x, y, x + rawWidth, y + buttonHeight);
    x = rawButtonRect_.right + buttonGap;
    previewButtonRect_ = MakeRect(x, y, x + previewWidth, y + buttonHeight);
    x = previewButtonRect_.right + buttonGap;
    darkButtonRect_ = MakeRect(x, y, x + darkWidth, y + buttonHeight);

    RECT content = client;
    content.top += commandHeight;
    content.bottom -= statusHeight;
    editor_.Resize(content);
    preview_.Resize(content);

    if (commandBar_ != nullptr) {
        InvalidateRect(commandBar_, nullptr, TRUE);
    }
}

const Theme& AppWindow::GetTheme() const noexcept {
    return GetCurrentTheme(themeMode_);
}

void AppWindow::ApplyTheme() {
    const Theme& theme = GetTheme();
    editor_.ApplyTheme(theme);
    preview_.ApplyTheme(theme);
    RecreateCommandBarFont();
    RecreateStatusBarFont();
    ApplyWindowTheme();
    ResizeChildren();
    UpdateMenuState();
    UpdateStatusBar();
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void AppWindow::ApplyWindowTheme() {
    if (hwnd_ == nullptr) {
        return;
    }

    const Theme& theme = GetTheme();
    const BOOL darkMode = IsDarkTheme(themeMode_) ? TRUE : FALSE;
    const DWM_WINDOW_CORNER_PREFERENCE cornerPreference = DWMWCP_ROUND;
    const DWM_SYSTEMBACKDROP_TYPE backdrop = DWMSBT_MAINWINDOW;
    DwmSetWindowAttribute(hwnd_, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
    DwmSetWindowAttribute(hwnd_, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPreference, sizeof(cornerPreference));
    DwmSetWindowAttribute(hwnd_, DWMWA_CAPTION_COLOR, &theme.titleBarColor, sizeof(theme.titleBarColor));
    DwmSetWindowAttribute(hwnd_, DWMWA_TEXT_COLOR, &theme.titleBarTextColor, sizeof(theme.titleBarTextColor));
    DwmSetWindowAttribute(hwnd_, DWMWA_BORDER_COLOR, &theme.windowBorder, sizeof(theme.windowBorder));
    DwmSetWindowAttribute(hwnd_, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
}

void AppWindow::RecreateCommandBarFont() {
    if (commandBarFont_ != nullptr) {
        DeleteObject(commandBarFont_);
        commandBarFont_ = nullptr;
    }
    commandBarFont_ = CreateUiFont(commandBar_ != nullptr ? commandBar_ : hwnd_, GetTheme().uiFontName, 9, FW_SEMIBOLD);
}

void AppWindow::RecreateStatusBarFont() {
    if (statusFont_ != nullptr) {
        DeleteObject(statusFont_);
        statusFont_ = nullptr;
    }
    statusFont_ = CreateUiFont(statusBar_ != nullptr ? statusBar_ : hwnd_, GetTheme().uiFontName, 9);
}

void AppWindow::UpdateTitle() {
    std::wstring title = std::wstring(Localize(UiString::AppName)) + L" - " + document_.GetDisplayName();
    if (document_.IsDirty()) {
        title += L" *";
    }
    SetWindowTextW(hwnd_, title.c_str());
}

void AppWindow::UpdateMenuState() {
    if (commandBar_ != nullptr) {
        InvalidateRect(commandBar_, nullptr, TRUE);
    }
}

void AppWindow::UpdateStatusBar() {
    if (statusBar_ == nullptr || editor_.GetHandle() == nullptr) {
        return;
    }

    const EditorStatus status = editor_.GetStatus();
    wchar_t buffer[96] = {};
    swprintf_s(buffer, Localize(UiString::StatusLineColumnFormat), status.line, status.column);
    statusPrimary_ = buffer;
    swprintf_s(buffer, Localize(UiString::StatusCountsFormat), status.characterCount, status.lineCount);
    statusSecondary_ = buffer;
    statusTertiary_ = LocalizeWide(UiString::StatusEncodingUtf8);
    InvalidateRect(statusBar_, nullptr, TRUE);
}

RECT AppWindow::GetCommandButtonRect(int commandId) const noexcept {
    switch (commandId) {
    case kCommandFile:
        return fileButtonRect_;
    case ID_VIEW_RAW:
        return rawButtonRect_;
    case ID_VIEW_PREVIEW:
        return previewButtonRect_;
    case ID_VIEW_DARKMODE:
        return darkButtonRect_;
    default:
        return RECT{};
    }
}

int AppWindow::HitTestCommandBar(POINT point) const noexcept {
    struct Entry {
        int id;
        RECT rect;
    };
    const Entry entries[] = {
        {kCommandFile, fileButtonRect_},
        {ID_VIEW_RAW, rawButtonRect_},
        {ID_VIEW_PREVIEW, previewButtonRect_},
        {ID_VIEW_DARKMODE, darkButtonRect_},
    };
    for (const Entry& entry : entries) {
        if (PtInRect(&entry.rect, point)) {
            return entry.id;
        }
    }
    return 0;
}

void AppWindow::PaintCommandBar(HDC hdc) {
    RECT client{};
    GetClientRect(commandBar_, &client);
    const Theme& theme = GetTheme();
    FillSolidRect(hdc, client, theme.titleBarColor);

    RECT bottomBorder{0, client.bottom - 1, client.right, client.bottom};
    FillSolidRect(hdc, bottomBorder, theme.windowBorder);

    SetBkMode(hdc, TRANSPARENT);
    if (commandBarFont_ != nullptr) {
        SelectObject(hdc, commandBarFont_);
    }

    auto drawButton = [&](int commandId, const RECT& rect, const wchar_t* text, bool selected) {
        COLORREF fill = theme.titleBarColor;
        COLORREF border = theme.titleBarColor;
        COLORREF textColor = theme.titleBarTextColor;

        if (selected) {
            fill = theme.accentColor;
            border = theme.accentColor;
            textColor = RGB(255, 255, 255);
        } else if (pressedCommandId_ == commandId) {
            fill = theme.windowBorder;
            border = theme.windowBorder;
        } else if (hotCommandId_ == commandId) {
            fill = theme.statusBarBackground;
            border = theme.windowBorder;
        }

        FillSolidRect(hdc, rect, fill);
        FrameRectSolid(hdc, rect, border);
        SetTextColor(hdc, textColor);
        RECT textRect = rect;
        DrawTextW(hdc, text, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        if (commandId == kCommandFile) {
            RECT arrowRect = rect;
            arrowRect.left = arrowRect.right - ScaleForWindow(commandBar_, 18);
            DrawTextW(hdc, L"v", -1, &arrowRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    };

    drawButton(kCommandFile, fileButtonRect_, Localize(UiString::CommandFile), false);
    drawButton(ID_VIEW_RAW, rawButtonRect_, Localize(UiString::CommandRaw), viewMode_ == ViewMode::Raw);
    drawButton(ID_VIEW_PREVIEW, previewButtonRect_, Localize(UiString::CommandPreview), viewMode_ == ViewMode::Preview);
    drawButton(ID_VIEW_DARKMODE, darkButtonRect_, Localize(UiString::CommandDarkMode), IsDarkTheme(themeMode_));
}

void AppWindow::PaintStatusBar(HDC hdc) {
    RECT client{};
    GetClientRect(statusBar_, &client);
    const Theme& theme = GetTheme();
    FillSolidRect(hdc, client, theme.statusBarBackground);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, theme.statusBarText);

    HPEN borderPen = CreatePen(PS_SOLID, 1, theme.statusBarBorder);
    HGDIOBJ previousPen = SelectObject(hdc, borderPen);
    MoveToEx(hdc, 0, 0, nullptr);
    LineTo(hdc, client.right, 0);
    const int inset = ScaleForWindow(statusBar_, 6);
    const int firstSeparator = client.right / 3;
    const int secondSeparator = (client.right * 2) / 3;
    MoveToEx(hdc, firstSeparator, inset, nullptr);
    LineTo(hdc, firstSeparator, client.bottom - inset);
    MoveToEx(hdc, secondSeparator, inset, nullptr);
    LineTo(hdc, secondSeparator, client.bottom - inset);
    SelectObject(hdc, previousPen);
    DeleteObject(borderPen);

    HGDIOBJ previousFont = nullptr;
    if (statusFont_ != nullptr) {
        previousFont = SelectObject(hdc, statusFont_);
    }

    const int padding = ScaleForWindow(statusBar_, 12);
    RECT leftRect{padding, 0, firstSeparator - padding, client.bottom};
    RECT centerRect{firstSeparator + padding, 0, secondSeparator - padding, client.bottom};
    RECT rightRect{secondSeparator + padding, 0, client.right - padding, client.bottom};
    DrawTextW(hdc, statusPrimary_.c_str(), -1, &leftRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    DrawTextW(hdc, statusSecondary_.c_str(), -1, &centerRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SetTextColor(hdc, theme.statusBarMutedText);
    DrawTextW(hdc, statusTertiary_.c_str(), -1, &rightRect, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    if (previousFont != nullptr) {
        SelectObject(hdc, previousFont);
    }
}

void AppWindow::ShowFileMenu() {
    if (fileMenu_ == nullptr || commandBar_ == nullptr) {
        return;
    }

    POINT point{fileButtonRect_.left, fileButtonRect_.bottom};
    ClientToScreen(commandBar_, &point);
    TrackPopupMenuEx(fileMenu_, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON, point.x, point.y, hwnd_, nullptr);
}

void AppWindow::SetDirty(bool dirty) {
    document_.SetDirty(dirty);
    UpdateTitle();
}

void AppWindow::SyncDocumentFromEditor() {
    document_.SetText(editor_.GetTextUtf8());
}

void AppWindow::SyncPreviewFromDocument() {
    preview_.SetDocumentText(document_.GetText());
}

void AppWindow::SetViewMode(ViewMode mode) {
    if (mode == viewMode_) {
        return;
    }
    if (mode == ViewMode::Preview) {
        SyncDocumentFromEditor();
        SyncPreviewFromDocument();
        editor_.Show(false);
        preview_.Show(true);
        preview_.Focus();
    } else {
        preview_.Show(false);
        editor_.Show(true);
        editor_.Focus();
    }
    viewMode_ = mode;
    UpdateMenuState();
    UpdateStatusBar();
}

void AppWindow::ToggleDarkMode() {
    themeMode_ = IsDarkTheme(themeMode_) ? ThemeMode::Light : ThemeMode::Dark;
    ApplyTheme();
}

bool AppWindow::ConfirmDiscardChanges() {
    if (!document_.IsDirty()) {
        return true;
    }
    const int result = MessageBoxW(hwnd_, Localize(UiString::PromptSaveBeforeContinue), Localize(UiString::AppName),
                                   MB_ICONWARNING | MB_YESNOCANCEL);
    if (result == IDYES) {
        return DoSaveDocument(false);
    }
    return result == IDNO;
}

bool AppWindow::DoNewDocument() {
    if (!ConfirmDiscardChanges()) {
        return false;
    }
    document_.NewDocument();
    editor_.SetTextUtf8({});
    editor_.MarkClean();
    SetDirty(false);
    SetViewMode(ViewMode::Raw);
    UpdateStatusBar();
    UpdateTitle();
    return true;
}

bool AppWindow::DoOpenDocument(const std::wstring& requestedPath) {
    if (!ConfirmDiscardChanges()) {
        return false;
    }
    std::wstring path = requestedPath.empty() ? PromptForOpenPath() : requestedPath;
    if (path.empty()) {
        return false;
    }

    std::wstring error;
    if (!document_.LoadFromFile(path, error)) {
        ShowError(error);
        return false;
    }

    editor_.SetTextUtf8(document_.GetText());
    editor_.MarkClean();
    SyncPreviewFromDocument();
    SetDirty(false);
    SetViewMode(ViewMode::Raw);
    UpdateStatusBar();
    UpdateTitle();
    return true;
}

bool AppWindow::DoSaveDocument(bool saveAs) {
    SyncDocumentFromEditor();
    std::wstring path = document_.GetPath();
    if (saveAs || path.empty()) {
        path = PromptForSavePath();
        if (path.empty()) {
            return false;
        }
    }

    std::wstring error;
    if (!document_.SaveToFile(path, document_.GetText(), error)) {
        ShowError(error);
        return false;
    }

    editor_.MarkClean();
    SetDirty(false);
    SyncPreviewFromDocument();
    UpdateStatusBar();
    UpdateTitle();
    return true;
}

void AppWindow::ShowError(const std::wstring& message) {
    MessageBoxW(hwnd_, message.c_str(), Localize(UiString::AppName), MB_ICONERROR | MB_OK);
}

std::wstring AppWindow::PromptForOpenPath() {
    wchar_t buffer[MAX_PATH] = {};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = hwnd_;
    dialog.lpstrFilter = Localize(UiString::FileDialogFilter);
    dialog.lpstrFile = buffer;
    dialog.nMaxFile = MAX_PATH;
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
    dialog.lpstrDefExt = L"md";
    return GetOpenFileNameW(&dialog) ? buffer : L"";
}

std::wstring AppWindow::PromptForSavePath() {
    wchar_t buffer[MAX_PATH] = {};
    if (document_.HasPath()) {
        wcsncpy_s(buffer, document_.GetPath().c_str(), _TRUNCATE);
    } else {
        wcsncpy_s(buffer, Localize(UiString::DefaultUntitledName), _TRUNCATE);
    }

    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = hwnd_;
    dialog.lpstrFilter = Localize(UiString::FileDialogFilter);
    dialog.lpstrFile = buffer;
    dialog.nMaxFile = MAX_PATH;
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    dialog.lpstrDefExt = L"md";
    return GetSaveFileNameW(&dialog) ? buffer : L"";
}

LRESULT CALLBACK AppWindow::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    AppWindow* self = nullptr;
    if (message == WM_NCCREATE) {
        auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<AppWindow*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<AppWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    return self != nullptr ? self->HandleMessage(message, wParam, lParam)
                           : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK AppWindow::CommandBarProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    AppWindow* self = nullptr;
    if (message == WM_NCCREATE) {
        auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<AppWindow*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->commandBar_ = hwnd;
    } else {
        self = reinterpret_cast<AppWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    return self != nullptr ? self->HandleCommandBarMessage(message, wParam, lParam)
                           : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK AppWindow::StatusBarProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    AppWindow* self = nullptr;
    if (message == WM_NCCREATE) {
        auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<AppWindow*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->statusBar_ = hwnd;
    } else {
        self = reinterpret_cast<AppWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    return self != nullptr ? self->HandleStatusBarMessage(message, wParam, lParam)
                           : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT AppWindow::HandleCommandBarMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSELEAVE:
        hotCommandId_ = 0;
        InvalidateRect(commandBar_, nullptr, TRUE);
        return 0;
    case WM_MOUSEMOVE: {
        TRACKMOUSEEVENT event{sizeof(event), TME_LEAVE, commandBar_, 0};
        TrackMouseEvent(&event);
        const int hotId = HitTestCommandBar({GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
        if (hotId != hotCommandId_) {
            hotCommandId_ = hotId;
            InvalidateRect(commandBar_, nullptr, TRUE);
        }
        return 0;
    }
    case WM_LBUTTONDOWN:
        pressedCommandId_ = HitTestCommandBar({GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
        SetCapture(commandBar_);
        InvalidateRect(commandBar_, nullptr, TRUE);
        return 0;
    case WM_LBUTTONUP: {
        const int hitId = HitTestCommandBar({GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
        const int pressedId = pressedCommandId_;
        pressedCommandId_ = 0;
        if (GetCapture() == commandBar_) {
            ReleaseCapture();
        }
        InvalidateRect(commandBar_, nullptr, TRUE);
        if (hitId != 0 && hitId == pressedId) {
            if (hitId == kCommandFile) {
                ShowFileMenu();
            } else {
                SendMessageW(hwnd_, WM_COMMAND, MAKEWPARAM(hitId, 0), 0);
            }
        }
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(commandBar_, &ps);
        PaintCommandBar(hdc);
        EndPaint(commandBar_, &ps);
        return 0;
    }
    default:
        return DefWindowProcW(commandBar_, message, wParam, lParam);
    }
}

LRESULT AppWindow::HandleStatusBarMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(statusBar_, &ps);
        PaintStatusBar(hdc);
        EndPaint(statusBar_, &ps);
        return 0;
    }
    default:
        return DefWindowProcW(statusBar_, message, wParam, lParam);
    }
}

LRESULT AppWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        if (!editor_.Create(hwnd_, instance_, ID_EDITOR) ||
            !preview_.Create(hwnd_, instance_, ID_PREVIEW) ||
            !CreateCommandBar() ||
            !CreateStatusBar()) {
            return -1;
        }
        document_.NewDocument();
        editor_.SetTextUtf8({});
        preview_.Show(false);
        ResizeChildren();
        UpdateStatusBar();
        UpdateTitle();
        return 0;
    case WM_ERASEBKGND:
        if (wParam != 0) {
            RECT client{};
            GetClientRect(hwnd_, &client);
            FillSolidRect(reinterpret_cast<HDC>(wParam), client, GetTheme().windowBackground);
        }
        return 1;
    case WM_SIZE:
        ResizeChildren();
        return 0;
    case WM_DPICHANGED: {
        const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                     suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
        ApplyTheme();
        return 0;
    }
    case WM_THEMECHANGED:
    case WM_SYSCOLORCHANGE:
    case WM_SETTINGCHANGE:
        if (themeMode_ == ThemeMode::System) {
            ApplyTheme();
        }
        return 0;
    case WM_SETFOCUS:
        if (viewMode_ == ViewMode::Raw) {
            editor_.Focus();
        } else {
            preview_.Focus();
        }
        UpdateStatusBar();
        return 0;
    case WM_NOTIFY:
        if (editor_.IsNotificationFrom(reinterpret_cast<NMHDR*>(lParam))) {
            const auto* notification = reinterpret_cast<SCNotification*>(lParam);
            if (!editor_.IsLoading()) {
                if (notification->nmhdr.code == SCN_SAVEPOINTLEFT) {
                    SetDirty(true);
                } else if (notification->nmhdr.code == SCN_SAVEPOINTREACHED) {
                    SetDirty(false);
                }
            }
            if (notification->nmhdr.code == SCN_UPDATEUI ||
                notification->nmhdr.code == SCN_SAVEPOINTLEFT ||
                notification->nmhdr.code == SCN_SAVEPOINTREACHED) {
                UpdateStatusBar();
            }
            return 0;
        }
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_FILE_NEW: DoNewDocument(); return 0;
        case ID_FILE_OPEN: DoOpenDocument(); return 0;
        case ID_FILE_SAVE: DoSaveDocument(false); return 0;
        case ID_FILE_SAVE_AS: DoSaveDocument(true); return 0;
        case ID_FILE_EXIT: SendMessageW(hwnd_, WM_CLOSE, 0, 0); return 0;
        case ID_VIEW_TOGGLE_PREVIEW: SetViewMode(viewMode_ == ViewMode::Raw ? ViewMode::Preview : ViewMode::Raw); return 0;
        case ID_VIEW_RAW: SetViewMode(ViewMode::Raw); return 0;
        case ID_VIEW_PREVIEW: SetViewMode(ViewMode::Preview); return 0;
        case ID_VIEW_DARKMODE: ToggleDarkMode(); return 0;
        default: break;
        }
        break;
    case WM_CLOSE:
        if (!ConfirmDiscardChanges()) {
            return 0;
        }
        DestroyWindow(hwnd_);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd_, message, wParam, lParam);
}
