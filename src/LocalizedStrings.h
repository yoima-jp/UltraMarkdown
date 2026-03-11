#pragma once

#include <string>

enum class UiString {
    AppName,
    ErrorScintillaInit,
    MenuFile,
    MenuNew,
    MenuOpen,
    MenuSave,
    MenuSaveAs,
    MenuExit,
    MenuRaw,
    MenuPreview,
    PromptSaveBeforeContinue,
    FileDialogFilter,
    DefaultUntitledName,
    ErrorOpenFile,
    ErrorFileSize,
    ErrorReadFile,
    ErrorCreateFile,
    ErrorWriteFile,
    ErrorParseMarkdown,
    PreviewEmptyDocument,
    PreviewInlineImage,
};

bool IsJapaneseUi() noexcept;
const wchar_t* Localize(UiString id) noexcept;
std::wstring LocalizeWide(UiString id);
