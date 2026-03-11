#pragma once

#include <string>

std::wstring Utf8ToWide(const std::string& utf8);
std::string WideToUtf8(const std::wstring& wide);
std::wstring GetBaseName(const std::wstring& path);
