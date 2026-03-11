#include "Utf8.h"

#include <windows.h>
#include <shlwapi.h>

std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) {
        return {};
    }

    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(),
                                         static_cast<int>(utf8.size()), nullptr, 0);
    if (size <= 0) {
        return {};
    }

    std::wstring wide(size, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(),
                        static_cast<int>(utf8.size()), wide.data(), size);
    return wide;
}

std::string WideToUtf8(const std::wstring& wide) {
    if (wide.empty()) {
        return {};
    }

    const int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(),
                                         static_cast<int>(wide.size()), nullptr, 0,
                                         nullptr, nullptr);
    if (size <= 0) {
        return {};
    }

    std::string utf8(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                        utf8.data(), size, nullptr, nullptr);
    return utf8;
}

std::wstring GetBaseName(const std::wstring& path) {
    if (path.empty()) {
        return L"Untitled.md";
    }

    const wchar_t* file = PathFindFileNameW(path.c_str());
    return file != nullptr ? std::wstring(file) : path;
}
