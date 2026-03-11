#pragma once

#include "MarkdownRenderer.h"

#include <string>
#include <vector>

#include <windows.h>

class PreviewView {
public:
    bool Create(HWND parent, HINSTANCE instance, int controlId);
    void Resize(const RECT& bounds);
    void Show(bool visible);
    void SetDocumentText(const std::string& markdownUtf8);
    HWND GetHandle() const noexcept { return hwnd_; }

private:
    struct LayoutRun {
        int x = 0;
        int y = 0;
        int height = 0;
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
        std::vector<LayoutRun> runs;
    };

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void CreateFonts();
    void DestroyFonts();
    void RebuildLayout();
    void UpdateScrollBar();
    void ScrollTo(int position);
    void Paint();

    HWND hwnd_ = nullptr;
    MarkdownRenderer renderer_;
    PreviewDocument document_;
    std::wstring errorText_;
    std::vector<LayoutBlock> layout_;
    int scrollY_ = 0;
    int contentHeight_ = 0;
    HFONT bodyFont_ = nullptr;
    HFONT bodyBoldFont_ = nullptr;
    HFONT bodyItalicFont_ = nullptr;
    HFONT bodyBoldItalicFont_ = nullptr;
    HFONT codeFont_ = nullptr;
    HFONT heading1Font_ = nullptr;
    HFONT heading2Font_ = nullptr;
    HFONT heading3Font_ = nullptr;
};
