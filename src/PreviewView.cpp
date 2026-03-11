#include "PreviewView.h"

#include <algorithm>
#include <cwctype>

namespace {
constexpr wchar_t kPreviewClassName[] = L"UltraMarkdownPreview";
constexpr int kOuterPadding = 18;
constexpr int kBlockSpacing = 12;
constexpr int kListIndent = 26;
constexpr int kQuoteIndent = 16;
constexpr int kCodePaddingX = 10;
constexpr int kCodePaddingY = 8;

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
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    hwnd_ = CreateWindowExW(0, kPreviewClassName, L"", WS_CHILD | WS_VSCROLL | WS_CLIPCHILDREN,
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

void PreviewView::SetDocumentText(const std::string& markdownUtf8) {
    errorText_.clear();
    if (!renderer_.Build(markdownUtf8, document_, errorText_)) {
        document_.blocks.clear();
    }
    scrollY_ = 0;
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
    case WM_PAINT:
        Paint();
        return 0;
    case WM_NCDESTROY:
        DestroyFonts();
        return DefWindowProcW(hwnd_, message, wParam, lParam);
    default:
        return DefWindowProcW(hwnd_, message, wParam, lParam);
    }
}

void PreviewView::CreateFonts() {
    DestroyFonts();
    bodyFont_ = CreateFontForWindow(hwnd_, 11, FW_NORMAL, false, L"Segoe UI");
    bodyBoldFont_ = CreateFontForWindow(hwnd_, 11, FW_BOLD, false, L"Segoe UI");
    bodyItalicFont_ = CreateFontForWindow(hwnd_, 11, FW_NORMAL, true, L"Segoe UI");
    bodyBoldItalicFont_ = CreateFontForWindow(hwnd_, 11, FW_BOLD, true, L"Segoe UI");
    codeFont_ = CreateFontForWindow(hwnd_, 10, FW_NORMAL, false, L"Consolas");
    heading1Font_ = CreateFontForWindow(hwnd_, 18, FW_BOLD, false, L"Segoe UI");
    heading2Font_ = CreateFontForWindow(hwnd_, 15, FW_BOLD, false, L"Segoe UI");
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

    RECT client{};
    GetClientRect(hwnd_, &client);
    const int clientWidth = std::max(1, static_cast<int>(client.right - client.left));
    const int usableRight = clientWidth - kOuterPadding;
    int y = kOuterPadding;

    HDC hdc = GetDC(hwnd_);
    if (hdc == nullptr) {
        return;
    }

    auto fontForBlock = [&](const PreviewBlock& block, const PreviewTextStyle& style) -> HFONT {
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
    };

    auto layoutTokens = [&](LayoutBlock& block, const std::vector<Token>& tokens, int left, int right) {
        const int gapAfterPrefix = block.source.prefix.empty() ? 0 : 8;
        const HFONT prefixFont = fontForBlock(block.source, {});
        block.prefixWidth = block.source.prefix.empty()
                                ? 0
                                : MeasureString(hdc, prefixFont, block.source.prefix).cx + gapAfterPrefix;

        int lineStart = left + block.prefixWidth;
        int continuationStart = lineStart;
        int currentX = lineStart;
        int currentY = y;
        int currentLineTop = y;
        int currentLineHeight = std::max(FontHeight(hdc, prefixFont), FontHeight(hdc, bodyFont_));
        bool lineHasContent = false;
        bool firstLine = true;

        auto flushLine = [&]() {
            currentY += currentLineHeight;
            currentLineTop = currentY;
            currentLineHeight = std::max(FontHeight(hdc, prefixFont), FontHeight(hdc, bodyFont_));
            currentX = continuationStart;
            lineHasContent = false;
            firstLine = false;
        };

        auto addFragment = [&](const std::wstring& fragment, const PreviewTextStyle& style) {
            const HFONT font = fontForBlock(block.source, style);
            const int fragmentHeight = FontHeight(hdc, font);
            block.runs.push_back({currentX, currentLineTop, fragmentHeight, fragment, style});
            currentX += MeasureString(hdc, font, fragment).cx;
            currentLineHeight = std::max(currentLineHeight, fragmentHeight);
            lineHasContent = true;
        };

        auto fitFragment = [&](const std::wstring& tokenText, const PreviewTextStyle& style) {
            if (tokenText.empty()) {
                return;
            }

            HFONT font = fontForBlock(block.source, style);
            SIZE tokenSize = MeasureString(hdc, font, tokenText);
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
                    SIZE fragmentSize = MeasureString(hdc, font, tokenText.substr(start, count));
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
            currentY += currentLineHeight;
        }
        y = currentY;
    };

    if (!errorText_.empty()) {
        LayoutBlock errorBlock;
        errorBlock.source.type = PreviewBlockType::Paragraph;
        errorBlock.source.spans.push_back({errorText_, {}});
        layoutTokens(errorBlock, TokenizeParagraph(errorBlock.source.spans), kOuterPadding, usableRight);
        errorBlock.top = kOuterPadding;
        errorBlock.bottom = y;
        layout_.push_back(std::move(errorBlock));
    } else if (document_.blocks.empty()) {
        LayoutBlock emptyBlock;
        emptyBlock.source.type = PreviewBlockType::Paragraph;
        emptyBlock.source.spans.push_back({L"(empty document)", {}});
        emptyBlock.top = y;
        layoutTokens(emptyBlock, TokenizeParagraph(emptyBlock.source.spans), kOuterPadding, usableRight);
        emptyBlock.bottom = y;
        layout_.push_back(std::move(emptyBlock));
    } else {
        for (const PreviewBlock& source : document_.blocks) {
            LayoutBlock block;
            block.source = source;
            block.top = y;

            const int quoteOffset = source.quoteDepth * kQuoteIndent;
            const int indentOffset = source.indentLevel * kListIndent;
            const int left = kOuterPadding + quoteOffset + indentOffset;
            block.contentLeft = left;

            if (source.type == PreviewBlockType::ThematicBreak) {
                block.drawRule = true;
                block.ruleY = y + 7;
                y += 14;
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
            y += kBlockSpacing;
        }
    }

    ReleaseDC(hwnd_, hdc);
    contentHeight_ = std::max(y, static_cast<int>(client.bottom - client.top));
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

void PreviewView::Paint() {
    PAINTSTRUCT ps{};
    HDC hdc = BeginPaint(hwnd_, &ps);

    RECT client{};
    GetClientRect(hwnd_, &client);
    FillRect(hdc, &client, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
    SetBkMode(hdc, TRANSPARENT);

    const COLORREF textColor = RGB(34, 39, 46);
    const COLORREF quoteColor = RGB(210, 214, 219);
    const COLORREF ruleColor = RGB(214, 219, 224);
    const COLORREF codeBack = RGB(246, 248, 250);
    const COLORREF codeText = RGB(100, 30, 22);
    const COLORREF linkColor = RGB(9, 105, 218);

    for (const LayoutBlock& block : layout_) {
        const int drawTop = block.top - scrollY_;
        const int drawBottom = block.bottom - scrollY_;
        if (drawBottom < 0 || drawTop > client.bottom) {
            continue;
        }

        if (block.source.quoteDepth > 0) {
            for (int depth = 0; depth < block.source.quoteDepth; ++depth) {
                RECT bar{
                    kOuterPadding + block.source.indentLevel * kListIndent + depth * kQuoteIndent,
                    drawTop,
                    kOuterPadding + block.source.indentLevel * kListIndent + depth * kQuoteIndent + 4,
                    drawBottom
                };
                HBRUSH brush = CreateSolidBrush(quoteColor);
                FillRect(hdc, &bar, brush);
                DeleteObject(brush);
            }
        }

        if (block.drawBackground) {
            RECT background = block.backgroundRect;
            background.top -= scrollY_;
            background.bottom -= scrollY_;
            HBRUSH brush = CreateSolidBrush(codeBack);
            FillRect(hdc, &background, brush);
            DeleteObject(brush);
            HBRUSH borderBrush = CreateSolidBrush(RGB(222, 226, 230));
            FrameRect(hdc, &background, borderBrush);
            DeleteObject(borderBrush);
        }

        if (block.drawRule) {
            HPEN pen = CreatePen(PS_SOLID, 1, ruleColor);
            HGDIOBJ previousPen = SelectObject(hdc, pen);
            MoveToEx(hdc, block.contentLeft, block.ruleY - scrollY_, nullptr);
            LineTo(hdc, client.right - kOuterPadding, block.ruleY - scrollY_);
            SelectObject(hdc, previousPen);
            DeleteObject(pen);
        }

        if (!block.source.prefix.empty()) {
            HGDIOBJ previous = SelectObject(hdc, bodyBoldFont_);
            SetTextColor(hdc, textColor);
            TextOutW(hdc, block.contentLeft, block.top - scrollY_,
                     block.source.prefix.c_str(), static_cast<int>(block.source.prefix.size()));
            SelectObject(hdc, previous);
        }

        for (const LayoutRun& run : block.runs) {
            HFONT font = bodyFont_;
            if (run.style.code) {
                font = codeFont_;
            } else if (block.source.type == PreviewBlockType::Heading) {
                if (block.source.headingLevel <= 1) {
                    font = heading1Font_;
                } else if (block.source.headingLevel == 2) {
                    font = heading2Font_;
                } else {
                    font = heading3Font_;
                }
            } else if (run.style.bold && run.style.italic) {
                font = bodyBoldItalicFont_;
            } else if (run.style.bold) {
                font = bodyBoldFont_;
            } else if (run.style.italic) {
                font = bodyItalicFont_;
            }

            HGDIOBJ previous = SelectObject(hdc, font);
            if (run.style.link) {
                SetTextColor(hdc, linkColor);
            } else if (run.style.code) {
                SetTextColor(hdc, codeText);
            } else {
                SetTextColor(hdc, textColor);
            }
            TextOutW(hdc, run.x, run.y - scrollY_, run.text.c_str(), static_cast<int>(run.text.size()));
            SelectObject(hdc, previous);
        }
    }

    EndPaint(hwnd_, &ps);
}
