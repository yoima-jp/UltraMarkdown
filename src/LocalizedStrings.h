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
    MenuSetDefaultMarkdownApp,
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
    ErrorRegisterDefaultApp,
    ErrorOpenDefaultAppsSettings,
    ErrorParseMarkdown,
    PreviewEmptyDocument,
    PreviewInlineImage,
    ContextUndo,
    ContextRedo,
    ContextCut,
    ContextCopy,
    ContextPaste,
    ContextDelete,
    ContextSelectAll,
    StatusLineColumnFormat,
    StatusCountsFormat,
    StatusEncodingUtf8,
};

bool IsJapaneseUi() noexcept;
const wchar_t* Localize(UiString id) noexcept;
std::wstring LocalizeWide(UiString id);
