#include "AppWindow.h"

namespace {
std::wstring Trim(std::wstring value) {
    const auto first = value.find_first_not_of(L" \t");
    if (first == std::wstring::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(L" \t");
    value = value.substr(first, last - first + 1);

    if (value.size() >= 2 && value.front() == L'"' && value.back() == L'"') {
        value = value.substr(1, value.size() - 2);
    }

    return value;
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int showCommand) {
    std::wstring startupPath;
    if (commandLine != nullptr) {
        startupPath = Trim(commandLine);
    }

    AppWindow app(instance, startupPath);
    return app.Run(showCommand);
}
