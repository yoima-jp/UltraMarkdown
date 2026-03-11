#pragma once

#include "Document.h"
#include "EditorHost.h"
#include "PreviewView.h"

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
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    bool CreateMainWindow(int showCommand);
    void CreateMenus();
    void CreateAccelerators();
    void ResizeChildren();

    void UpdateTitle();
    void SetDirty(bool dirty);
    void SyncDocumentFromEditor();
    void SyncPreviewFromDocument();
    void SetViewMode(ViewMode mode);
    bool ConfirmDiscardChanges();

    bool DoNewDocument();
    bool DoOpenDocument(const std::wstring& path = {});
    bool DoSaveDocument(bool saveAs);
    void ShowError(const std::wstring& message);
    std::wstring PromptForOpenPath();
    std::wstring PromptForSavePath();

    HWND hwnd_ = nullptr;
    HINSTANCE instance_ = nullptr;
    HACCEL accelerators_ = nullptr;
    Document document_;
    EditorHost editor_;
    PreviewView preview_;
    ViewMode viewMode_ = ViewMode::Raw;
    std::wstring startupPath_;
};
