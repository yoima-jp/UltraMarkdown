#pragma once

#include <string>
#include <vector>

enum class PreviewBlockType {
    Paragraph,
    Heading,
    ListItem,
    CodeBlock,
    ThematicBreak,
};

struct PreviewTextStyle {
    bool bold = false;
    bool italic = false;
    bool code = false;
    bool link = false;

    bool operator==(const PreviewTextStyle& other) const noexcept {
        return bold == other.bold &&
               italic == other.italic &&
               code == other.code &&
               link == other.link;
    }
};

struct PreviewSpan {
    std::wstring text;
    PreviewTextStyle style;
};

struct PreviewBlock {
    PreviewBlockType type = PreviewBlockType::Paragraph;
    int headingLevel = 0;
    int indentLevel = 0;
    int quoteDepth = 0;
    std::wstring prefix;
    std::wstring codeText;
    std::vector<PreviewSpan> spans;
};

struct PreviewDocument {
    std::vector<PreviewBlock> blocks;
};

class MarkdownRenderer {
public:
    bool Build(const std::string& markdownUtf8, PreviewDocument& output, std::wstring& error) const;
};
