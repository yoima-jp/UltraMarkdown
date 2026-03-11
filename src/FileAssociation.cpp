#include "FileAssociation.h"

#include "LocalizedStrings.h"

#include <shellapi.h>
#include <shlobj.h>
#include <string>
#include <vector>

namespace {
constexpr wchar_t kRegisteredAppName[] = L"UltraMarkdown";
constexpr wchar_t kCapabilitiesPath[] = L"Software\\UltraMarkdown\\Capabilities";
constexpr wchar_t kProgId[] = L"UltraMarkdown.MarkdownFile";
constexpr wchar_t kFileTypeDescription[] = L"Markdown Document";
constexpr wchar_t kApplicationDescription[] = L"Ultra-lightweight native Markdown editor";
constexpr wchar_t kMarkdownExtension[] = L".md";
constexpr wchar_t kMarkdownAltExtension[] = L".markdown";

std::wstring BuildErrorMessage(UiString messageId, LONG errorCode) {
    std::wstring message = LocalizeWide(messageId);

    wchar_t* systemMessage = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD length = FormatMessageW(flags, nullptr, static_cast<DWORD>(errorCode), 0,
                                        reinterpret_cast<LPWSTR>(&systemMessage), 0, nullptr);
    if (length == 0 || systemMessage == nullptr) {
        return message;
    }

    std::wstring details(systemMessage, length);
    LocalFree(systemMessage);
    while (!details.empty() && (details.back() == L'\r' || details.back() == L'\n' || details.back() == L' ')) {
        details.pop_back();
    }

    if (!details.empty()) {
        message.append(L"\n\n");
        message.append(details);
    }
    return message;
}

std::wstring GetExecutablePath() {
    std::vector<wchar_t> buffer(MAX_PATH);
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return {};
        }
        if (length < buffer.size() - 1) {
            return std::wstring(buffer.data(), length);
        }
        buffer.resize(buffer.size() * 2);
    }
}

bool WriteStringValue(HKEY rootKey,
                      const std::wstring& subKey,
                      const wchar_t* valueName,
                      const std::wstring& value,
                      UiString errorId,
                      std::wstring& error) {
    HKEY key = nullptr;
    const LONG openResult =
        RegCreateKeyExW(rootKey, subKey.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &key, nullptr);
    if (openResult != ERROR_SUCCESS) {
        error = BuildErrorMessage(errorId, openResult);
        return false;
    }

    const BYTE* data = reinterpret_cast<const BYTE*>(value.c_str());
    const DWORD dataSize = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
    const LONG setResult = RegSetValueExW(key, valueName, 0, REG_SZ, data, dataSize);
    RegCloseKey(key);
    if (setResult != ERROR_SUCCESS) {
        error = BuildErrorMessage(errorId, setResult);
        return false;
    }
    return true;
}

bool RegisterMarkdownApp(const std::wstring& executablePath, std::wstring& error) {
    const std::wstring command = L"\"" + executablePath + L"\" \"%1\"";
    const std::wstring iconPath = executablePath + L",0";

    if (!WriteStringValue(HKEY_CURRENT_USER, L"Software\\RegisteredApplications", kRegisteredAppName, kCapabilitiesPath,
                          UiString::ErrorRegisterDefaultApp, error)) {
        return false;
    }

    if (!WriteStringValue(HKEY_CURRENT_USER, kCapabilitiesPath, L"ApplicationName", kRegisteredAppName,
                          UiString::ErrorRegisterDefaultApp, error) ||
        !WriteStringValue(HKEY_CURRENT_USER, kCapabilitiesPath, L"ApplicationDescription", kApplicationDescription,
                          UiString::ErrorRegisterDefaultApp, error) ||
        !WriteStringValue(HKEY_CURRENT_USER, kCapabilitiesPath + std::wstring(L"\\FileAssociations"), kMarkdownExtension, kProgId,
                          UiString::ErrorRegisterDefaultApp, error) ||
        !WriteStringValue(HKEY_CURRENT_USER, kCapabilitiesPath + std::wstring(L"\\FileAssociations"), kMarkdownAltExtension,
                          kProgId, UiString::ErrorRegisterDefaultApp, error)) {
        return false;
    }

    const std::wstring appKey = std::wstring(L"Software\\Classes\\Applications\\") + kRegisteredAppName + L".exe";
    if (!WriteStringValue(HKEY_CURRENT_USER, appKey, L"FriendlyAppName", kRegisteredAppName,
                          UiString::ErrorRegisterDefaultApp, error) ||
        !WriteStringValue(HKEY_CURRENT_USER, appKey + L"\\shell\\open\\command", nullptr, command,
                          UiString::ErrorRegisterDefaultApp, error) ||
        !WriteStringValue(HKEY_CURRENT_USER, appKey + L"\\SupportedTypes", kMarkdownExtension, L"",
                          UiString::ErrorRegisterDefaultApp, error) ||
        !WriteStringValue(HKEY_CURRENT_USER, appKey + L"\\SupportedTypes", kMarkdownAltExtension, L"",
                          UiString::ErrorRegisterDefaultApp, error)) {
        return false;
    }

    const std::wstring progIdKey = std::wstring(L"Software\\Classes\\") + kProgId;
    if (!WriteStringValue(HKEY_CURRENT_USER, progIdKey, nullptr, kFileTypeDescription,
                          UiString::ErrorRegisterDefaultApp, error) ||
        !WriteStringValue(HKEY_CURRENT_USER, progIdKey + L"\\DefaultIcon", nullptr, iconPath,
                          UiString::ErrorRegisterDefaultApp, error) ||
        !WriteStringValue(HKEY_CURRENT_USER, progIdKey + L"\\shell\\open", nullptr, L"open",
                          UiString::ErrorRegisterDefaultApp, error) ||
        !WriteStringValue(HKEY_CURRENT_USER, progIdKey + L"\\shell\\open\\command", nullptr, command,
                          UiString::ErrorRegisterDefaultApp, error)) {
        return false;
    }

    const std::wstring mdOpenWithKey = std::wstring(L"Software\\Classes\\") + kMarkdownExtension + L"\\OpenWithProgids";
    const std::wstring markdownOpenWithKey = std::wstring(L"Software\\Classes\\") + kMarkdownAltExtension + L"\\OpenWithProgids";
    if (!WriteStringValue(HKEY_CURRENT_USER, mdOpenWithKey, kProgId, L"", UiString::ErrorRegisterDefaultApp, error) ||
        !WriteStringValue(HKEY_CURRENT_USER, markdownOpenWithKey, kProgId, L"", UiString::ErrorRegisterDefaultApp, error)) {
        return false;
    }

    return true;
}

bool LaunchDefaultAppsSettings(HWND owner) {
    const std::wstring appSpecificUri = std::wstring(L"ms-settings:defaultapps?registeredAppUser=") + kRegisteredAppName;
    const INT_PTR appSpecificResult =
        reinterpret_cast<INT_PTR>(ShellExecuteW(owner, L"open", appSpecificUri.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    if (appSpecificResult > 32) {
        return true;
    }

    const INT_PTR genericResult =
        reinterpret_cast<INT_PTR>(ShellExecuteW(owner, L"open", L"ms-settings:defaultapps", nullptr, nullptr, SW_SHOWNORMAL));
    return genericResult > 32;
}
}

DefaultMarkdownAppResult RegisterAsDefaultMarkdownApp(HWND owner) {
    DefaultMarkdownAppResult result;

    const std::wstring executablePath = GetExecutablePath();
    if (executablePath.empty()) {
        result.error = LocalizeWide(UiString::ErrorRegisterDefaultApp);
        return result;
    }

    if (!RegisterMarkdownApp(executablePath, result.error)) {
        return result;
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

    if (!LaunchDefaultAppsSettings(owner)) {
        result.error = LocalizeWide(UiString::ErrorOpenDefaultAppsSettings);
        return result;
    }

    result.success = true;
    return result;
}
