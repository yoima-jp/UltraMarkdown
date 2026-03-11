#pragma once

#include "MarkdownRenderer.h"
#include "Theme.h"

#include <string>
#include <vector>

#include <windows.h>

class PreviewView {
public:
    bool Create(HWND parent, HINSTANCE instance, int controlId);
    void Resize(const RECT& bounds);
    void Show(bool visible);
    void Focus();
    void ApplyTheme(const Theme& theme);
    void SetDocumentText(const std::string& markdownUtf8);
    HWND GetHandle() const noexcept { return hwnd_; }

private:
    struct LayoutRun {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        int textStart = 0;
        std::wstring text;
        PreviewTextStyle style;
    };

    struct LayoutBlock {
        PreviewBlock source;
        int top = 0;
        int bottom = 0;
        int contentLeft = 0;
        int prefixWidth = 0;
        int ruleY = 0;
        bool drawRule = false;
        RECT backgroundRect{};
        bool drawBackground = false;
        int textStart = 0;
        int textLength = 0;
        std::vector<LayoutRun> runs;
    };

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void CreateFonts();
    void DestroyFonts();
    void RebuildLayout();
    void UpdateScrollBar();
    void ScrollTo(int position);
    HFONT ResolveFont(const PreviewBlock& block, const PreviewTextStyle& style) const noexcept;
    int HitTestTextPosition(POINT clientPoint) const;
    int HitTestTextPosition(HDC hdc, POINT clientPoint) const;
    int HitTestRunPosition(HDC hdc, const LayoutRun& run, const PreviewBlock& block, int clientX) const;
    void SetSelection(int anchor, int focus);
    void ClearSelection();
    bool HasSelection() const noexcept;
    std::wstring GetSelectedText() const;
    void CopySelectionToClipboard() const;
    void Paint();

    HWND hwnd_ = nullptr;
    MarkdownRenderer renderer_;
    PreviewDocument document_;
    std::wstring errorText_;
    std::wstring plainText_;
    std::vector<LayoutBlock> layout_;
    int scrollY_ = 0;
    int contentHeight_ = 0;
    int selectionAnchor_ = -1;
    int selectionFocus_ = -1;
    bool selecting_ = false;
    HFONT bodyFont_ = nullptr;
    HFONT bodyBoldFont_ = nullptr;
    HFONT bodyItalicFont_ = nullptr;
    HFONT bodyBoldItalicFont_ = nullptr;
    HFONT codeFont_ = nullptr;
    HFONT heading1Font_ = nullptr;
    HFONT heading2Font_ = nullptr;
    HFONT heading3Font_ = nullptr;
    Theme theme_ = GetCurrentTheme();
};
