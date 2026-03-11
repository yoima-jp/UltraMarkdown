#include "PreviewView.h"

#include <algorithm>
#include <cstring>
#include <cwctype>
#include <windowsx.h>

namespace {
constexpr wchar_t kPreviewClassName[] = L"UltraMarkdownPreview";
constexpr int kOuterPadding = 24;
constexpr int kBlockSpacing = 14;
constexpr int kListIndent = 26;
constexpr int kQuoteIndent = 16;
constexpr int kCodePaddingX = 12;
constexpr int kCodePaddingY = 10;
constexpr int kMaxContentWidth = 920;

struct Token {
    std::wstring text;
    PreviewTextStyle style;
    bool whitespace = false;
    bool forceBreak = false;
};

int GetDpiForWindowSafe(HWND hwnd) {
    if (hwnd == nullptr) {
        return 96;
    }
    return static_cast<int>(GetDpiForWindow(hwnd));
}

HFONT CreateFontForWindow(HWND hwnd, int pointSize, int weight, bool italic, const wchar_t* faceName) {
    LOGFONTW font{};
    font.lfHeight = -MulDiv(pointSize, GetDpiForWindowSafe(hwnd), 72);
    font.lfWeight = weight;
    font.lfItalic = italic ? TRUE : FALSE;
    font.lfQuality = CLEARTYPE_QUALITY;
    wcsncpy_s(font.lfFaceName, faceName, _TRUNCATE);
    return CreateFontIndirectW(&font);
}

SIZE MeasureString(HDC hdc, HFONT font, const std::wstring& text) {
    SIZE size{};
    HGDIOBJ previous = SelectObject(hdc, font);
    if (!text.empty()) {
        GetTextExtentPoint32W(hdc, text.c_str(), static_cast<int>(text.size()), &size);
    }
    SelectObject(hdc, previous);
    return size;
}

int FontHeight(HDC hdc, HFONT font) {
    TEXTMETRICW metrics{};
    HGDIOBJ previous = SelectObject(hdc, font);
    GetTextMetricsW(hdc, &metrics);
    SelectObject(hdc, previous);
    return metrics.tmHeight + metrics.tmExternalLeading;
}

void FillSolidRect(HDC hdc, const RECT& rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
}

std::wstring ExpandTabs(std::wstring text) {
    size_t position = 0;
    while ((position = text.find(L'\t', position)) != std::wstring::npos) {
        text.replace(position, 1, L"    ");
        position += 4;
    }
    return text;
}

std::vector<Token> TokenizeParagraph(const std::vector<PreviewSpan>& spans) {
    std::vector<Token> tokens;
    for (const PreviewSpan& span : spans) {
        std::wstring text = ExpandTabs(span.text);
        size_t start = 0;
        while (start < text.size()) {
            const wchar_t ch = text[start];
            if (ch == L'\n') {
                tokens.push_back({L"", span.style, false, true});
                ++start;
                continue;
            }

            const bool whitespace = std::iswspace(ch) != 0;
            size_t end = start + 1;
            while (end < text.size() && text[end] != L'\n' && (std::iswspace(text[end]) != 0) == whitespace) {
                ++end;
            }
            tokens.push_back({text.substr(start, end - start), span.style, whitespace, false});
            start = end;
        }
    }
    return tokens;
}

std::vector<Token> TokenizeCode(const std::wstring& text) {
    std::vector<Token> tokens;
    const PreviewTextStyle style{false, false, true, false};
    std::wstring expanded = ExpandTabs(text);
    size_t start = 0;
    while (start < expanded.size()) {
        size_t end = expanded.find(L'\n', start);
        if (end == std::wstring::npos) {
            end = expanded.size();
        }
        tokens.push_back({expanded.substr(start, end - start), style, false, false});
        if (end < expanded.size()) {
            tokens.push_back({L"", style, false, true});
        }
        start = end + 1;
    }
    if (tokens.empty()) {
        tokens.push_back({L"", style, false, false});
    }
    return tokens;
}
}

bool PreviewView::Create(HWND parent, HINSTANCE instance, int controlId) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = PreviewView::WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = kPreviewClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_IBEAM);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    hwnd_ = CreateWindowExW(0, kPreviewClassName, L"",
                            WS_CHILD | WS_VSCROLL | WS_CLIPCHILDREN | WS_TABSTOP,
                            0, 0, 0, 0, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
                            instance, this);
    if (!hwnd_) {
        return false;
    }

    CreateFonts();
    return true;
}

void PreviewView::Resize(const RECT& bounds) {
    MoveWindow(hwnd_, bounds.left, bounds.top, bounds.right - bounds.left,
               bounds.bottom - bounds.top, TRUE);
    RebuildLayout();
}

void PreviewView::Show(bool visible) {
    ShowWindow(hwnd_, visible ? SW_SHOW : SW_HIDE);
}

void PreviewView::Focus() {
    if (hwnd_ != nullptr) {
        SetFocus(hwnd_);
    }
}

void PreviewView::SetDocumentText(const std::string& markdownUtf8) {
    errorText_.clear();
    if (!renderer_.Build(markdownUtf8, document_, errorText_)) {
        document_.blocks.clear();
    }
    scrollY_ = 0;
    ClearSelection();
    RebuildLayout();
}

LRESULT CALLBACK PreviewView::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    PreviewView* self = nullptr;
    if (message == WM_NCCREATE) {
        auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<PreviewView*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<PreviewView*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    return self ? self->HandleMessage(message, wParam, lParam)
                : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT PreviewView::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_SIZE:
        RebuildLayout();
        return 0;

    case WM_GETDLGCODE:
        return DLGC_WANTARROWS;

    case WM_VSCROLL: {
        SCROLLINFO info{};
        info.cbSize = sizeof(info);
        info.fMask = SIF_ALL;
        GetScrollInfo(hwnd_, SB_VERT, &info);
        int next = info.nPos;
        switch (LOWORD(wParam)) {
        case SB_LINEUP:
            next -= 32;
            break;
        case SB_LINEDOWN:
            next += 32;
            break;
        case SB_PAGEUP:
            next -= static_cast<int>(info.nPage);
            break;
        case SB_PAGEDOWN:
            next += static_cast<int>(info.nPage);
            break;
        case SB_THUMBPOSITION:
        case SB_THUMBTRACK:
            next = info.nTrackPos;
            break;
        default:
            break;
        }
        ScrollTo(next);
        return 0;
    }

    case WM_MOUSEWHEEL:
        ScrollTo(scrollY_ - static_cast<short>(HIWORD(wParam)) / WHEEL_DELTA * 48);
        return 0;

    case WM_LBUTTONDOWN: {
        SetFocus(hwnd_);
        SetCapture(hwnd_);
        selecting_ = true;
        const int position = HitTestTextPosition({GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
        SetSelection(position, position);
        return 0;
    }

    case WM_MOUSEMOVE:
        if (selecting_ && (wParam & MK_LBUTTON) != 0) {
            SetSelection(selectionAnchor_, HitTestTextPosition({GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}));
        }
        return 0;

    case WM_LBUTTONUP:
        if (selecting_) {
            selecting_ = false;
            ReleaseCapture();
            SetSelection(selectionAnchor_, HitTestTextPosition({GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}));
        }
        return 0;

    case WM_KEYDOWN:
        if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) {
            if (wParam == 'C' || wParam == VK_INSERT) {
                CopySelectionToClipboard();
                return 0;
            }
            if (wParam == 'A') {
                SetSelection(0, static_cast<int>(plainText_.size()));
                return 0;
            }
        }
        break;

    case WM_COPY:
        CopySelectionToClipboard();
        return 0;

    case WM_PAINT:
        Paint();
        return 0;

    case WM_NCDESTROY:
        DestroyFonts();
        return DefWindowProcW(hwnd_, message, wParam, lParam);

    default:
        return DefWindowProcW(hwnd_, message, wParam, lParam);
    }

    return DefWindowProcW(hwnd_, message, wParam, lParam);
}

HFONT PreviewView::ResolveFont(const PreviewBlock& block, const PreviewTextStyle& style) const noexcept {
    if (style.code) {
        return codeFont_;
    }
    if (block.type == PreviewBlockType::Heading) {
        if (block.headingLevel <= 1) {
            return heading1Font_;
        }
        if (block.headingLevel == 2) {
            return heading2Font_;
        }
        return heading3Font_;
    }
    if (style.bold && style.italic) {
        return bodyBoldItalicFont_;
    }
    if (style.bold) {
        return bodyBoldFont_;
    }
    if (style.italic) {
        return bodyItalicFont_;
    }
    return bodyFont_;
}

void PreviewView::CreateFonts() {
    DestroyFonts();
    bodyFont_ = CreateFontForWindow(hwnd_, 11, FW_NORMAL, false, L"Segoe UI");
    bodyBoldFont_ = CreateFontForWindow(hwnd_, 11, FW_SEMIBOLD, false, L"Segoe UI");
    bodyItalicFont_ = CreateFontForWindow(hwnd_, 11, FW_NORMAL, true, L"Segoe UI");
    bodyBoldItalicFont_ = CreateFontForWindow(hwnd_, 11, FW_SEMIBOLD, true, L"Segoe UI");
    codeFont_ = CreateFontForWindow(hwnd_, 10, FW_NORMAL, false, L"Consolas");
    heading1Font_ = CreateFontForWindow(hwnd_, 20, FW_BOLD, false, L"Segoe UI");
    heading2Font_ = CreateFontForWindow(hwnd_, 16, FW_BOLD, false, L"Segoe UI");
    heading3Font_ = CreateFontForWindow(hwnd_, 13, FW_BOLD, false, L"Segoe UI");
}

void PreviewView::DestroyFonts() {
    for (HFONT* font : {&bodyFont_, &bodyBoldFont_, &bodyItalicFont_, &bodyBoldItalicFont_,
                        &codeFont_, &heading1Font_, &heading2Font_, &heading3Font_}) {
        if (*font != nullptr) {
            DeleteObject(*font);
            *font = nullptr;
        }
    }
}

void PreviewView::RebuildLayout() {
    if (hwnd_ == nullptr) {
        return;
    }

    layout_.clear();
    plainText_.clear();

    RECT client{};
    GetClientRect(hwnd_, &client);
    const int clientWidth = std::max(1, static_cast<int>(client.right - client.left));
    const int contentWidth = std::max(1, std::min(clientWidth - (kOuterPadding * 2), kMaxContentWidth));
    const int columnLeft = std::max(kOuterPadding, (clientWidth - contentWidth) / 2);
    const int usableRight = columnLeft + contentWidth;
    int y = kOuterPadding;

    HDC hdc = GetDC(hwnd_);
    if (hdc == nullptr) {
        return;
    }

    auto layoutTokens = [&](LayoutBlock& block, const std::vector<Token>& tokens, int left, int right) {
        std::wstring prefixText = block.source.prefix.empty() ? L"" : block.source.prefix + L" ";
        block.prefixWidth = prefixText.empty() ? 0 : MeasureString(hdc, bodyBoldFont_, prefixText).cx;

        int currentLineTop = y;
        int currentLineHeight = std::max(FontHeight(hdc, bodyFont_), FontHeight(hdc, bodyBoldFont_));
        int currentX = left + block.prefixWidth;
        const int continuationStart = currentX;
        bool lineHasContent = false;

        if (!prefixText.empty()) {
            const int textStart = static_cast<int>(plainText_.size());
            const int height = FontHeight(hdc, bodyBoldFont_);
            block.runs.push_back({left, currentLineTop, block.prefixWidth, height, textStart, prefixText,
                                  PreviewTextStyle{true, false, false, false}});
            plainText_ += prefixText;
            block.textLength = static_cast<int>(plainText_.size()) - block.textStart;
        }

        auto flushLine = [&]() {
            currentLineTop += currentLineHeight;
            currentLineHeight = std::max(FontHeight(hdc, bodyFont_), FontHeight(hdc, bodyBoldFont_));
            currentX = continuationStart;
            lineHasContent = false;
        };

        auto addFragment = [&](const std::wstring& fragment, const PreviewTextStyle& style) {
            if (fragment.empty()) {
                return;
            }

            const HFONT font = ResolveFont(block.source, style);
            const SIZE fragmentSize = MeasureString(hdc, font, fragment);
            const int fragmentHeight = FontHeight(hdc, font);
            const int textStart = static_cast<int>(plainText_.size());

            block.runs.push_back({currentX, currentLineTop, fragmentSize.cx, fragmentHeight, textStart, fragment, style});
            plainText_ += fragment;
            block.textLength = static_cast<int>(plainText_.size()) - block.textStart;
            currentX += fragmentSize.cx;
            currentLineHeight = std::max(currentLineHeight, fragmentHeight);
            lineHasContent = true;
        };

        auto fitFragment = [&](const std::wstring& tokenText, const PreviewTextStyle& style) {
            if (tokenText.empty()) {
                return;
            }

            const HFONT font = ResolveFont(block.source, style);
            const SIZE tokenSize = MeasureString(hdc, font, tokenText);
            if (currentX + tokenSize.cx <= right) {
                addFragment(tokenText, style);
                return;
            }

            if (lineHasContent) {
                flushLine();
            }

            size_t start = 0;
            while (start < tokenText.size()) {
                size_t best = 1;
                for (size_t count = 1; start + count <= tokenText.size(); ++count) {
                    const SIZE fragmentSize = MeasureString(hdc, font, tokenText.substr(start, count));
                    if (continuationStart + fragmentSize.cx > right) {
                        break;
                    }
                    best = count;
                }
                addFragment(tokenText.substr(start, best), style);
                start += best;
                if (start < tokenText.size()) {
                    flushLine();
                }
            }
        };

        for (const Token& token : tokens) {
            if (token.forceBreak) {
                plainText_ += L'\n';
                block.textLength = static_cast<int>(plainText_.size()) - block.textStart;
                flushLine();
                continue;
            }
            if (token.text.empty()) {
                continue;
            }
            if (token.whitespace && !lineHasContent) {
                continue;
            }
            fitFragment(token.text, token.style);
        }

        if (lineHasContent || block.runs.empty()) {
            currentLineTop += currentLineHeight;
        }
        y = currentLineTop;
    };

    auto layoutOneBlock = [&](LayoutBlock& block, const std::vector<Token>& tokens, int left, int right) {
        block.textStart = static_cast<int>(plainText_.size());
        layoutTokens(block, tokens, left, right);
        block.bottom = y;
    };

    if (!errorText_.empty()) {
        LayoutBlock errorBlock;
        errorBlock.source.type = PreviewBlockType::Paragraph;
        errorBlock.source.spans.push_back({errorText_, {}});
        errorBlock.top = y;
        layoutOneBlock(errorBlock, TokenizeParagraph(errorBlock.source.spans), columnLeft, usableRight);
        layout_.push_back(std::move(errorBlock));
    } else if (document_.blocks.empty()) {
        LayoutBlock emptyBlock;
        emptyBlock.source.type = PreviewBlockType::Paragraph;
        emptyBlock.source.spans.push_back({L"(empty document)", {}});
        emptyBlock.top = y;
        layoutOneBlock(emptyBlock, TokenizeParagraph(emptyBlock.source.spans), columnLeft, usableRight);
        layout_.push_back(std::move(emptyBlock));
    } else {
        for (size_t index = 0; index < document_.blocks.size(); ++index) {
            const PreviewBlock& source = document_.blocks[index];
            LayoutBlock block;
            block.source = source;
            block.top = y;
            block.textStart = static_cast<int>(plainText_.size());

            const int quoteOffset = source.quoteDepth * kQuoteIndent;
            const int indentOffset = source.indentLevel * kListIndent;
            const int left = columnLeft + quoteOffset + indentOffset;
            block.contentLeft = left;

            if (source.type == PreviewBlockType::ThematicBreak) {
                block.drawRule = true;
                block.ruleY = y + 8;
                y += 16;
            } else if (source.type == PreviewBlockType::CodeBlock) {
                y += kCodePaddingY;
                block.drawBackground = true;
                block.backgroundRect.left = left;
                block.backgroundRect.top = block.top;
                block.backgroundRect.right = usableRight;
                layoutTokens(block, TokenizeCode(source.codeText), left + kCodePaddingX, usableRight - kCodePaddingX);
                y += kCodePaddingY;
                block.backgroundRect.bottom = y;
            } else {
                layoutTokens(block, TokenizeParagraph(source.spans), left, usableRight);
            }

            block.bottom = y;
            layout_.push_back(std::move(block));

            if (index + 1 < document_.blocks.size()) {
                plainText_ += L'\n';
            }
            y += kBlockSpacing;
        }
    }

    ReleaseDC(hwnd_, hdc);

    if (plainText_.empty()) {
        ClearSelection();
    } else {
        selectionAnchor_ = std::clamp(selectionAnchor_, 0, static_cast<int>(plainText_.size()));
        selectionFocus_ = std::clamp(selectionFocus_, 0, static_cast<int>(plainText_.size()));
    }

    contentHeight_ = std::max(y + kOuterPadding, static_cast<int>(client.bottom - client.top));
    scrollY_ = std::clamp(scrollY_, 0, std::max(contentHeight_ - static_cast<int>(client.bottom - client.top), 0));
    UpdateScrollBar();
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void PreviewView::UpdateScrollBar() {
    RECT client{};
    GetClientRect(hwnd_, &client);

    SCROLLINFO info{};
    info.cbSize = sizeof(info);
    info.fMask = SIF_PAGE | SIF_RANGE | SIF_POS;
    info.nMin = 0;
    info.nMax = std::max(contentHeight_ - 1, 0);
    info.nPage = static_cast<UINT>(std::max(static_cast<int>(client.bottom - client.top), 0));
    info.nPos = scrollY_;
    SetScrollInfo(hwnd_, SB_VERT, &info, TRUE);
}

void PreviewView::ScrollTo(int position) {
    RECT client{};
    GetClientRect(hwnd_, &client);
    const int page = std::max(static_cast<int>(client.bottom - client.top), 0);
    const int maxScroll = std::max(contentHeight_ - page, 0);
    const int clamped = std::clamp(position, 0, maxScroll);
    if (clamped == scrollY_) {
        return;
    }

    scrollY_ = clamped;
    SCROLLINFO info{};
    info.cbSize = sizeof(info);
    info.fMask = SIF_POS;
    info.nPos = scrollY_;
    SetScrollInfo(hwnd_, SB_VERT, &info, TRUE);
    InvalidateRect(hwnd_, nullptr, TRUE);
}

int PreviewView::HitTestTextPosition(POINT clientPoint) const {
    HDC hdc = GetDC(hwnd_);
    if (hdc == nullptr) {
        return 0;
    }
    const int position = HitTestTextPosition(hdc, clientPoint);
    ReleaseDC(hwnd_, hdc);
    return position;
}

int PreviewView::HitTestTextPosition(HDC hdc, POINT clientPoint) const {
    if (layout_.empty() || plainText_.empty()) {
        return 0;
    }

    const int contentY = clientPoint.y + scrollY_;
    const LayoutBlock* targetBlock = &layout_.front();
    int bestDistance = INT_MAX;

    for (const LayoutBlock& block : layout_) {
        if (contentY >= block.top && contentY < block.bottom) {
            targetBlock = &block;
            bestDistance = 0;
            break;
        }

        const int distance = contentY < block.top ? block.top - contentY : contentY - block.bottom;
        if (distance < bestDistance) {
            bestDistance = distance;
            targetBlock = &block;
        }
    }

    if (targetBlock->runs.empty()) {
        return std::clamp(targetBlock->textStart, 0, static_cast<int>(plainText_.size()));
    }

    int targetLineY = targetBlock->runs.front().y;
    int targetLineDistance = INT_MAX;
    for (const LayoutRun& run : targetBlock->runs) {
        const int runBottom = run.y + run.height;
        const int distance = contentY < run.y ? run.y - contentY : (contentY > runBottom ? contentY - runBottom : 0);
        if (distance < targetLineDistance) {
            targetLineDistance = distance;
            targetLineY = run.y;
        }
    }

    const LayoutRun* firstRun = nullptr;
    const LayoutRun* lastRun = nullptr;
    for (const LayoutRun& run : targetBlock->runs) {
        if (run.y != targetLineY) {
            continue;
        }

        if (firstRun == nullptr) {
            firstRun = &run;
        }
        lastRun = &run;

        if (clientPoint.x < run.x) {
            return run.textStart;
        }
        if (clientPoint.x <= run.x + run.width) {
            return HitTestRunPosition(hdc, run, targetBlock->source, clientPoint.x);
        }
    }

    if (firstRun == nullptr) {
        return std::clamp(targetBlock->textStart + targetBlock->textLength, 0, static_cast<int>(plainText_.size()));
    }

    if (clientPoint.x <= firstRun->x) {
        return firstRun->textStart;
    }

    return lastRun->textStart + static_cast<int>(lastRun->text.size());
}

int PreviewView::HitTestRunPosition(HDC hdc, const LayoutRun& run, const PreviewBlock& block, int clientX) const {
    const HFONT font = ResolveFont(block, run.style);
    HGDIOBJ previous = SelectObject(hdc, font);

    int fit = 0;
    SIZE size{};
    const int relativeX = std::max(clientX - run.x, 0);
    GetTextExtentExPointW(hdc, run.text.c_str(), static_cast<int>(run.text.size()), relativeX, &fit, nullptr, &size);

    if (fit < static_cast<int>(run.text.size())) {
        SIZE nextSize{};
        GetTextExtentPoint32W(hdc, run.text.c_str(), fit + 1, &nextSize);
        const int previousWidth = size.cx;
        const int midpoint = previousWidth + ((nextSize.cx - previousWidth) / 2);
        if (relativeX >= midpoint) {
            ++fit;
        }
    }

    SelectObject(hdc, previous);
    return run.textStart + fit;
}

void PreviewView::SetSelection(int anchor, int focus) {
    const int maxPosition = static_cast<int>(plainText_.size());
    selectionAnchor_ = std::clamp(anchor, 0, maxPosition);
    selectionFocus_ = std::clamp(focus, 0, maxPosition);
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void PreviewView::ClearSelection() {
    selectionAnchor_ = 0;
    selectionFocus_ = 0;
    selecting_ = false;
}

bool PreviewView::HasSelection() const noexcept {
    return selectionAnchor_ != selectionFocus_;
}

std::wstring PreviewView::GetSelectedText() const {
    if (!HasSelection()) {
        return {};
    }

    const int start = std::min(selectionAnchor_, selectionFocus_);
    const int end = std::max(selectionAnchor_, selectionFocus_);
    return plainText_.substr(start, end - start);
}

void PreviewView::CopySelectionToClipboard() const {
    const std::wstring selected = GetSelectedText();
    if (selected.empty()) {
        return;
    }

    if (!OpenClipboard(hwnd_)) {
        return;
    }

    EmptyClipboard();

    const size_t bytes = (selected.size() + 1) * sizeof(wchar_t);
    HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (handle != nullptr) {
        void* memory = GlobalLock(handle);
        if (memory != nullptr) {
            memcpy(memory, selected.c_str(), bytes);
            GlobalUnlock(handle);
            SetClipboardData(CF_UNICODETEXT, handle);
            handle = nullptr;
        }
    }

    if (handle != nullptr) {
        GlobalFree(handle);
    }

    CloseClipboard();
}

void PreviewView::Paint() {
    PAINTSTRUCT ps{};
    HDC hdc = BeginPaint(hwnd_, &ps);

    RECT client{};
    GetClientRect(hwnd_, &client);
    FillSolidRect(hdc, client, RGB(241, 244, 248));
    SetBkMode(hdc, TRANSPARENT);

    const COLORREF bodyText = RGB(33, 40, 48);
    const COLORREF mutedText = RGB(112, 121, 132);
    const COLORREF quoteColor = RGB(211, 219, 228);
    const COLORREF ruleColor = RGB(216, 223, 231);
    const COLORREF codeBack = RGB(248, 250, 252);
    const COLORREF codeText = RGB(128, 53, 30);
    const COLORREF linkColor = RGB(14, 98, 191);
    const COLORREF selectionFill = RGB(206, 228, 255);
    const COLORREF selectionText = RGB(16, 54, 96);

    const int clientWidth = std::max(1, static_cast<int>(client.right - client.left));
    const int contentWidth = std::max(1, std::min(clientWidth - (kOuterPadding * 2), kMaxContentWidth));
    const int columnLeft = std::max(kOuterPadding, (clientWidth - contentWidth) / 2);
    RECT paperRect{columnLeft - 12, 12, columnLeft + contentWidth + 12,
                   std::max(static_cast<int>(client.bottom) - 12, 12)};
    FillSolidRect(hdc, paperRect, RGB(255, 255, 255));
    HBRUSH paperBorderBrush = CreateSolidBrush(RGB(227, 232, 238));
    FrameRect(hdc, &paperRect, paperBorderBrush);
    DeleteObject(paperBorderBrush);

    const int selectionStart = std::min(selectionAnchor_, selectionFocus_);
    const int selectionEnd = std::max(selectionAnchor_, selectionFocus_);

    for (const LayoutBlock& block : layout_) {
        const int drawTop = block.top - scrollY_;
        const int drawBottom = block.bottom - scrollY_;
        if (drawBottom < 0 || drawTop > client.bottom) {
            continue;
        }

        if (block.source.quoteDepth > 0) {
            for (int depth = 0; depth < block.source.quoteDepth; ++depth) {
                RECT bar{
                    columnLeft + block.source.indentLevel * kListIndent + depth * kQuoteIndent,
                    drawTop,
                    columnLeft + block.source.indentLevel * kListIndent + depth * kQuoteIndent + 4,
                    drawBottom
                };
                FillSolidRect(hdc, bar, quoteColor);
            }
        }

        if (block.drawBackground) {
            RECT background = block.backgroundRect;
            background.top -= scrollY_;
            background.bottom -= scrollY_;
            FillSolidRect(hdc, background, codeBack);
            HBRUSH borderBrush = CreateSolidBrush(RGB(226, 231, 237));
            FrameRect(hdc, &background, borderBrush);
            DeleteObject(borderBrush);
        }

        if (block.drawRule) {
            HPEN pen = CreatePen(PS_SOLID, 1, ruleColor);
            HGDIOBJ previousPen = SelectObject(hdc, pen);
            MoveToEx(hdc, block.contentLeft, block.ruleY - scrollY_, nullptr);
            LineTo(hdc, columnLeft + contentWidth, block.ruleY - scrollY_);
            SelectObject(hdc, previousPen);
            DeleteObject(pen);
        }

        for (const LayoutRun& run : block.runs) {
            HFONT font = ResolveFont(block.source, run.style);
            HGDIOBJ previous = SelectObject(hdc, font);

            const int runStart = run.textStart;
            const int runEnd = run.textStart + static_cast<int>(run.text.size());
            bool selected = HasSelection() && selectionStart < runEnd && selectionEnd > runStart;
            if (selected) {
                const int localStart = std::max(selectionStart - runStart, 0);
                const int localEnd = std::min(selectionEnd - runStart, static_cast<int>(run.text.size()));

                SIZE prefixSize{};
                if (localStart > 0) {
                    GetTextExtentPoint32W(hdc, run.text.c_str(), localStart, &prefixSize);
                }

                SIZE selectedSize{};
                const int selectedLength = std::max(localEnd - localStart, 0);
                if (selectedLength > 0) {
                    GetTextExtentPoint32W(hdc, run.text.c_str() + localStart, selectedLength, &selectedSize);
                    RECT selectionRect{
                        run.x + prefixSize.cx,
                        run.y - scrollY_,
                        run.x + prefixSize.cx + selectedSize.cx,
                        run.y - scrollY_ + run.height
                    };
                    FillSolidRect(hdc, selectionRect, selectionFill);
                }
            }

            COLORREF textColor = bodyText;
            if (run.style.link) {
                textColor = linkColor;
            } else if (run.style.code) {
                textColor = codeText;
            } else if (run.text == L"(empty document)") {
                textColor = mutedText;
            }
            if (selected) {
                textColor = selectionText;
            }

            SetTextColor(hdc, textColor);
            TextOutW(hdc, run.x, run.y - scrollY_, run.text.c_str(), static_cast<int>(run.text.size()));
            SelectObject(hdc, previous);
        }
    }

    if (GetFocus() == hwnd_) {
        RECT focusRect = paperRect;
        InflateRect(&focusRect, -2, -2);
        DrawFocusRect(hdc, &focusRect);
    }

    EndPaint(hwnd_, &ps);
}
