#include "AppWindow.h"

#include "Utf8.h"
#include "resource_ids.h"

#include <commdlg.h>
#include <shellapi.h>

namespace {
constexpr wchar_t kWindowClassName[] = L"UltraMarkdownMainWindow";
constexpr wchar_t kAppName[] = L"UltraMarkdown";
}

AppWindow::AppWindow(HINSTANCE instance, std::wstring startupPath)
    : instance_(instance), startupPath_(std::move(startupPath)) {
}

int AppWindow::Run(int showCommand) {
    if (!Scintilla_RegisterClasses(instance_)) {
        MessageBoxW(nullptr, L"Could not initialize Scintilla.", kAppName, MB_ICONERROR | MB_OK);
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
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    hwnd_ = CreateWindowExW(0, kWindowClassName, kAppName,
                            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                            CW_USEDEFAULT, CW_USEDEFAULT, 1100, 760,
                            nullptr, nullptr, instance_, this);
    if (!hwnd_) {
        return false;
    }

    CreateMenus();
    CreateAccelerators();

    ShowWindow(hwnd_, showCommand);
    UpdateWindow(hwnd_);
    return true;
}

void AppWindow::CreateMenus() {
    HMENU menuBar = CreateMenu();
    HMENU fileMenu = CreatePopupMenu();
    HMENU viewMenu = CreatePopupMenu();

    AppendMenuW(fileMenu, MF_STRING, ID_FILE_NEW, L"&New\tCtrl+N");
    AppendMenuW(fileMenu, MF_STRING, ID_FILE_OPEN, L"&Open...\tCtrl+O");
    AppendMenuW(fileMenu, MF_STRING, ID_FILE_SAVE, L"&Save\tCtrl+S");
    AppendMenuW(fileMenu, MF_STRING, ID_FILE_SAVE_AS, L"Save &As...\tCtrl+Shift+S");
    AppendMenuW(fileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(fileMenu, MF_STRING, ID_FILE_EXIT, L"E&xit");

    AppendMenuW(viewMenu, MF_STRING, ID_VIEW_TOGGLE_PREVIEW, L"&Toggle Preview\tF6");

    AppendMenuW(menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu), L"&File");
    AppendMenuW(menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(viewMenu), L"&View");
    SetMenu(hwnd_, menuBar);
}

void AppWindow::CreateAccelerators() {
    ACCEL entries[] = {
        {FVIRTKEY | FCONTROL, 'N', static_cast<WORD>(ID_FILE_NEW)},
        {FVIRTKEY | FCONTROL, 'O', static_cast<WORD>(ID_FILE_OPEN)},
        {FVIRTKEY | FCONTROL, 'S', static_cast<WORD>(ID_FILE_SAVE)},
        {FVIRTKEY | FCONTROL | FSHIFT, 'S', static_cast<WORD>(ID_FILE_SAVE_AS)},
        {FVIRTKEY, VK_F6, static_cast<WORD>(ID_VIEW_TOGGLE_PREVIEW)},
    };
    accelerators_ = CreateAcceleratorTableW(entries, static_cast<int>(std::size(entries)));
}

void AppWindow::ResizeChildren() {
    RECT client{};
    GetClientRect(hwnd_, &client);
    editor_.Resize(client);
    preview_.Resize(client);
}

void AppWindow::UpdateTitle() {
    std::wstring title = std::wstring(kAppName) + L" - " + document_.GetDisplayName();
    if (document_.IsDirty()) {
        title += L" *";
    }
    SetWindowTextW(hwnd_, title.c_str());
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
    } else {
        preview_.Show(false);
        editor_.Show(true);
        editor_.Focus();
    }

    viewMode_ = mode;
}

bool AppWindow::ConfirmDiscardChanges() {
    if (!document_.IsDirty()) {
        return true;
    }

    const int result = MessageBoxW(
        hwnd_,
        L"Save changes before continuing?",
        kAppName,
        MB_ICONWARNING | MB_YESNOCANCEL
    );

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
    UpdateTitle();
    return true;
}

void AppWindow::ShowError(const std::wstring& message) {
    MessageBoxW(hwnd_, message.c_str(), kAppName, MB_ICONERROR | MB_OK);
}

std::wstring AppWindow::PromptForOpenPath() {
    wchar_t buffer[MAX_PATH] = {};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = hwnd_;
    dialog.lpstrFilter = L"Markdown Files (*.md;*.markdown)\0*.md;*.markdown\0All Files (*.*)\0*.*\0";
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
        wcsncpy_s(buffer, L"Untitled.md", _TRUNCATE);
    }

    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = hwnd_;
    dialog.lpstrFilter = L"Markdown Files (*.md;*.markdown)\0*.md;*.markdown\0All Files (*.*)\0*.*\0";
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

    return self ? self->HandleMessage(message, wParam, lParam)
                : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT AppWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        if (!editor_.Create(hwnd_, instance_, ID_EDITOR)) {
            return -1;
        }
        if (!preview_.Create(hwnd_, instance_, ID_PREVIEW)) {
            return -1;
        }
        document_.NewDocument();
        editor_.SetTextUtf8({});
        preview_.Show(false);
        ResizeChildren();
        UpdateTitle();
        return 0;

    case WM_SIZE:
        ResizeChildren();
        return 0;

    case WM_SETFOCUS:
        if (viewMode_ == ViewMode::Raw) {
            editor_.Focus();
        }
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
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_FILE_NEW:
            DoNewDocument();
            return 0;
        case ID_FILE_OPEN:
            DoOpenDocument();
            return 0;
        case ID_FILE_SAVE:
            DoSaveDocument(false);
            return 0;
        case ID_FILE_SAVE_AS:
            DoSaveDocument(true);
            return 0;
        case ID_FILE_EXIT:
            SendMessageW(hwnd_, WM_CLOSE, 0, 0);
            return 0;
        case ID_VIEW_TOGGLE_PREVIEW:
            SetViewMode(viewMode_ == ViewMode::Raw ? ViewMode::Preview : ViewMode::Raw);
            return 0;
        default:
            break;
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
