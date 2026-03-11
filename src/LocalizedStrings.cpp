#include "LocalizedStrings.h"

#include <windows.h>

namespace {
bool IsJapanesePrimaryLanguage(LANGID languageId) noexcept {
    return PRIMARYLANGID(languageId) == LANG_JAPANESE;
}
}

bool IsJapaneseUi() noexcept {
    return IsJapanesePrimaryLanguage(GetUserDefaultUILanguage());
}

const wchar_t* Localize(UiString id) noexcept {
    const bool japanese = IsJapaneseUi();

    switch (id) {
    case UiString::AppName:
        return L"UltraMarkdown";
    case UiString::ErrorScintillaInit:
        return japanese ? L"Scintilla を初期化できませんでした。"
                        : L"Could not initialize Scintilla.";
    case UiString::MenuFile:
        return japanese ? L"ファイル(&F)" : L"&File";
    case UiString::MenuNew:
        return japanese ? L"新規作成(&N)\tCtrl+N" : L"&New\tCtrl+N";
    case UiString::MenuOpen:
        return japanese ? L"開く(&O)...\tCtrl+O" : L"&Open...\tCtrl+O";
    case UiString::MenuSave:
        return japanese ? L"保存(&S)\tCtrl+S" : L"&Save\tCtrl+S";
    case UiString::MenuSaveAs:
        return japanese ? L"名前を付けて保存(&A)...\tCtrl+Shift+S" : L"Save &As...\tCtrl+Shift+S";
    case UiString::MenuExit:
        return japanese ? L"終了(&X)" : L"E&xit";
    case UiString::MenuRaw:
        return japanese ? L"編集" : L"&Raw";
    case UiString::MenuPreview:
        return japanese ? L"プレビュー" : L"&Preview";
    case UiString::PromptSaveBeforeContinue:
        return japanese ? L"続行する前に変更を保存しますか？"
                        : L"Save changes before continuing?";
    case UiString::FileDialogFilter:
        return japanese
                   ? L"Markdown ファイル (*.md;*.markdown)\0*.md;*.markdown\0すべてのファイル (*.*)\0*.*\0"
                   : L"Markdown Files (*.md;*.markdown)\0*.md;*.markdown\0All Files (*.*)\0*.*\0";
    case UiString::DefaultUntitledName:
        return japanese ? L"無題.md" : L"Untitled.md";
    case UiString::ErrorOpenFile:
        return japanese ? L"ファイルを開けませんでした。"
                        : L"Could not open the file.";
    case UiString::ErrorFileSize:
        return japanese ? L"ファイルサイズを取得できませんでした。"
                        : L"Could not determine the file size.";
    case UiString::ErrorReadFile:
        return japanese ? L"ファイルを読み込めませんでした。"
                        : L"Could not read the file.";
    case UiString::ErrorCreateFile:
        return japanese ? L"ファイルを作成できませんでした。"
                        : L"Could not create the file.";
    case UiString::ErrorWriteFile:
        return japanese ? L"ファイルを書き込めませんでした。"
                        : L"Could not write the file.";
    case UiString::ErrorParseMarkdown:
        return japanese ? L"Markdown ドキュメントを解析できませんでした。"
                        : L"Could not parse the markdown document.";
    case UiString::PreviewEmptyDocument:
        return japanese ? L"(空のドキュメント)" : L"(empty document)";
    case UiString::PreviewInlineImage:
        return japanese ? L"[画像] " : L"[image] ";
    default:
        return L"";
    }
}

std::wstring LocalizeWide(UiString id) {
    return Localize(id);
}
