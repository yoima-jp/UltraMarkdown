#include "MarkdownRenderer.h"

#include "LocalizedStrings.h"
#include "Utf8.h"

#include <cmark-gfm.h>

namespace {
struct BlockContext {
    int indentLevel = 0;
    int quoteDepth = 0;
    std::wstring prefix;
};

std::wstring LiteralToWide(cmark_node* node) {
    const char* literal = cmark_node_get_literal(node);
    return literal != nullptr ? Utf8ToWide(literal) : std::wstring{};
}

void AppendSpan(std::vector<PreviewSpan>& spans, std::wstring text, const PreviewTextStyle& style) {
    if (text.empty()) {
        return;
    }

    if (!spans.empty() && spans.back().style == style) {
        spans.back().text += text;
        return;
    }

    spans.push_back({std::move(text), style});
}

void CollectInlineSpans(cmark_node* node, PreviewTextStyle style, std::vector<PreviewSpan>& spans) {
    if (node == nullptr) {
        return;
    }

    switch (cmark_node_get_type(node)) {
    case CMARK_NODE_TEXT:
    case CMARK_NODE_HTML_INLINE:
        AppendSpan(spans, LiteralToWide(node), style);
        return;
    case CMARK_NODE_SOFTBREAK:
        AppendSpan(spans, L" ", style);
        return;
    case CMARK_NODE_LINEBREAK:
        AppendSpan(spans, L"\n", style);
        return;
    case CMARK_NODE_CODE: {
        PreviewTextStyle codeStyle = style;
        codeStyle.code = true;
        AppendSpan(spans, LiteralToWide(node), codeStyle);
        return;
    }
    case CMARK_NODE_EMPH:
        style.italic = true;
        break;
    case CMARK_NODE_STRONG:
        style.bold = true;
        break;
    case CMARK_NODE_LINK:
        style.link = true;
        break;
    case CMARK_NODE_IMAGE:
        AppendSpan(spans, LocalizeWide(UiString::PreviewInlineImage), style);
        break;
    default:
        break;
    }

    for (cmark_node* child = cmark_node_first_child(node); child != nullptr; child = cmark_node_next(child)) {
        CollectInlineSpans(child, style, spans);
    }
}

void EmitLeafBlock(cmark_node* node, const BlockContext& context, PreviewDocument& output) {
    PreviewBlock block;
    block.indentLevel = context.indentLevel;
    block.quoteDepth = context.quoteDepth;
    block.prefix = context.prefix;

    switch (cmark_node_get_type(node)) {
    case CMARK_NODE_HEADING:
        block.type = PreviewBlockType::Heading;
        block.headingLevel = cmark_node_get_heading_level(node);
        break;
    case CMARK_NODE_PARAGRAPH:
        block.type = PreviewBlockType::Paragraph;
        break;
    case CMARK_NODE_CODE_BLOCK:
        block.type = PreviewBlockType::CodeBlock;
        block.codeText = LiteralToWide(node);
        output.blocks.push_back(std::move(block));
        return;
    case CMARK_NODE_THEMATIC_BREAK:
        block.type = PreviewBlockType::ThematicBreak;
        output.blocks.push_back(std::move(block));
        return;
    case CMARK_NODE_HTML_BLOCK:
        block.type = PreviewBlockType::CodeBlock;
        block.codeText = LiteralToWide(node);
        output.blocks.push_back(std::move(block));
        return;
    default:
        return;
    }

    for (cmark_node* child = cmark_node_first_child(node); child != nullptr; child = cmark_node_next(child)) {
        CollectInlineSpans(child, {}, block.spans);
    }

    if (!block.spans.empty() || !block.prefix.empty()) {
        output.blocks.push_back(std::move(block));
    }
}

void EmitBlocks(cmark_node* node, const BlockContext& context, PreviewDocument& output);

void EmitList(cmark_node* listNode, const BlockContext& context, PreviewDocument& output) {
    const cmark_list_type listType = cmark_node_get_list_type(listNode);
    int ordinal = cmark_node_get_list_start(listNode);
    for (cmark_node* item = cmark_node_first_child(listNode); item != nullptr; item = cmark_node_next(item)) {
        std::wstring prefix = listType == CMARK_ORDERED_LIST
                                  ? std::to_wstring(ordinal++) + L"."
                                  : L"*";
        const int itemIndentLevel = context.indentLevel + 1;
        bool firstBlock = true;

        for (cmark_node* child = cmark_node_first_child(item); child != nullptr; child = cmark_node_next(child)) {
            if (cmark_node_get_type(child) == CMARK_NODE_LIST) {
                EmitList(child, BlockContext{itemIndentLevel, context.quoteDepth, L""}, output);
                firstBlock = false;
                continue;
            }

            if (cmark_node_get_type(child) == CMARK_NODE_BLOCK_QUOTE) {
                EmitBlocks(child, BlockContext{itemIndentLevel, context.quoteDepth, firstBlock ? prefix : L""}, output);
                firstBlock = false;
                continue;
            }

            EmitLeafBlock(child, BlockContext{firstBlock ? context.indentLevel : itemIndentLevel,
                                              context.quoteDepth,
                                              firstBlock ? prefix : L""}, output);
            firstBlock = false;
        }
    }
}

void EmitBlocks(cmark_node* node, const BlockContext& context, PreviewDocument& output) {
    for (cmark_node* current = node; current != nullptr; current = cmark_node_next(current)) {
        switch (cmark_node_get_type(current)) {
        case CMARK_NODE_DOCUMENT:
            EmitBlocks(cmark_node_first_child(current), context, output);
            break;
        case CMARK_NODE_BLOCK_QUOTE:
            EmitBlocks(cmark_node_first_child(current),
                       BlockContext{context.indentLevel, context.quoteDepth + 1, context.prefix}, output);
            break;
        case CMARK_NODE_LIST:
            EmitList(current, context, output);
            break;
        default:
            EmitLeafBlock(current, context, output);
            break;
        }
    }
}
}

bool MarkdownRenderer::Build(const std::string& markdownUtf8, PreviewDocument& output, std::wstring& error) const {
    output.blocks.clear();
    error.clear();

    cmark_node* root = cmark_parse_document(markdownUtf8.c_str(), markdownUtf8.size(), CMARK_OPT_DEFAULT);
    if (root == nullptr) {
        error = LocalizeWide(UiString::ErrorParseMarkdown);
        return false;
    }

    EmitBlocks(root, {}, output);
    cmark_node_free(root);
    return true;
}
