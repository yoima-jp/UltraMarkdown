#pragma once

#include <string>

class Document {
public:
    void NewDocument();
    bool LoadFromFile(const std::wstring& path, std::wstring& error);
    bool SaveToFile(const std::wstring& path, const std::string& textUtf8, std::wstring& error);

    void SetText(std::string textUtf8);
    const std::string& GetText() const noexcept { return textUtf8_; }

    void SetPath(std::wstring path);
    const std::wstring& GetPath() const noexcept { return path_; }
    bool HasPath() const noexcept { return !path_.empty(); }

    void SetDirty(bool dirty) noexcept { dirty_ = dirty; }
    bool IsDirty() const noexcept { return dirty_; }

    std::wstring GetDisplayName() const;

private:
    static bool ReadFileUtf8(const std::wstring& path, std::string& output, std::wstring& error);
    static bool WriteFileUtf8(const std::wstring& path, const std::string& input, std::wstring& error);

    std::wstring path_;
    std::string textUtf8_;
    bool dirty_ = false;
};
