#pragma once

#include <string>

enum class UiString {
    AppName,
    ErrorScintillaInit,
    MenuFile,
    MenuView,
    MenuNew,
    MenuOpen,
    MenuSave,
    MenuSaveAs,
    MenuExit,
    MenuRaw,
    MenuPreview,
    MenuTogglePreview,
    MenuDarkMode,
    CommandFile,
    CommandRaw,
    CommandPreview,
    CommandDarkMode,
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
    StatusLineColumnFormat,
    StatusCountsFormat,
    StatusEncodingUtf8,
};

bool IsJapaneseUi() noexcept;
const wchar_t* Localize(UiString id) noexcept;
std::wstring LocalizeWide(UiString id);
