#include "Document.h"

#include "LocalizedStrings.h"
#include "Utf8.h"

#include <windows.h>

namespace {
constexpr unsigned char kUtf8Bom[] = {0xEF, 0xBB, 0xBF};
}

void Document::NewDocument() {
    path_.clear();
    textUtf8_.clear();
    dirty_ = false;
}

bool Document::LoadFromFile(const std::wstring& path, std::wstring& error) {
    std::string buffer;
    if (!ReadFileUtf8(path, buffer, error)) {
        return false;
    }

    path_ = path;
    textUtf8_ = std::move(buffer);
    dirty_ = false;
    return true;
}

bool Document::SaveToFile(const std::wstring& path, const std::string& textUtf8, std::wstring& error) {
    if (!WriteFileUtf8(path, textUtf8, error)) {
        return false;
    }

    path_ = path;
    textUtf8_ = textUtf8;
    dirty_ = false;
    return true;
}

void Document::SetText(std::string textUtf8) {
    textUtf8_ = std::move(textUtf8);
}

void Document::SetPath(std::wstring path) {
    path_ = std::move(path);
}

std::wstring Document::GetDisplayName() const {
    return GetBaseName(path_);
}

bool Document::ReadFileUtf8(const std::wstring& path, std::string& output, std::wstring& error) {
    HANDLE handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        error = LocalizeWide(UiString::ErrorOpenFile);
        return false;
    }

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(handle, &size) || size.QuadPart < 0 ||
        static_cast<ULONGLONG>(size.QuadPart) > static_cast<ULONGLONG>(SIZE_MAX)) {
        CloseHandle(handle);
        error = LocalizeWide(UiString::ErrorFileSize);
        return false;
    }

    output.resize(static_cast<size_t>(size.QuadPart));
    DWORD bytesRead = 0;
    const BOOL ok = output.empty() ? TRUE
                                   : ReadFile(handle, output.data(), static_cast<DWORD>(output.size()), &bytesRead, nullptr);
    CloseHandle(handle);

    if (!ok || bytesRead != output.size()) {
        error = LocalizeWide(UiString::ErrorReadFile);
        return false;
    }

    if (output.size() >= 3 &&
        static_cast<unsigned char>(output[0]) == kUtf8Bom[0] &&
        static_cast<unsigned char>(output[1]) == kUtf8Bom[1] &&
        static_cast<unsigned char>(output[2]) == kUtf8Bom[2]) {
        output.erase(0, 3);
    }

    return true;
}

bool Document::WriteFileUtf8(const std::wstring& path, const std::string& input, std::wstring& error) {
    HANDLE handle = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        error = LocalizeWide(UiString::ErrorCreateFile);
        return false;
    }

    DWORD bytesWritten = 0;
    const BOOL ok = input.empty() ? TRUE
                                  : WriteFile(handle, input.data(), static_cast<DWORD>(input.size()), &bytesWritten, nullptr);
    CloseHandle(handle);

    if (!ok || bytesWritten != input.size()) {
        error = LocalizeWide(UiString::ErrorWriteFile);
        return false;
    }

    return true;
}
