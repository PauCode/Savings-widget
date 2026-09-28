#include "StartupSettings.h"
#include "AppPaths.h"

#include <filesystem>
#include <fstream>
#include <string>

#include <windows.h>

namespace {
constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kValueName[] = L"PauCode Savings Jar";

std::filesystem::path GetSettingsPath() {
    const auto dataDirectory = AppPaths::DataDirectory();
    return dataDirectory.empty()
        ? std::filesystem::path{}
        : dataDirectory / L"start-with-windows.txt";
}

bool SetRunEntry(bool enabled) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(
            HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0, KEY_SET_VALUE,
            nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return false;
    }

    LONG result = ERROR_SUCCESS;
    if (enabled) {
        const auto executable = AppPaths::ExecutableDirectory() / L"MoneySavingWidget.exe";
        if (!std::filesystem::is_regular_file(executable)) {
            RegCloseKey(key);
            return false;
        }
        const std::wstring command = L"\"" + executable.wstring() + L"\"";
        result = RegSetValueExW(
            key, kValueName, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(command.c_str()),
            static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    } else {
        result = RegDeleteValueW(key, kValueName);
        if (result == ERROR_FILE_NOT_FOUND) {
            result = ERROR_SUCCESS;
        }
    }

    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}
} // namespace

void StartupSettings::Load() {
    answered_ = false;
    enabled_ = false;

    std::ifstream file(GetSettingsPath());
    std::string value;
    if (!file || !std::getline(file, value)) {
        return;
    }

    if (value == "enabled") {
        enabled_ = SetRunEntry(true);
        answered_ = enabled_;
    } else if (value == "disabled") {
        answered_ = true;
        enabled_ = false;
        SetRunEntry(false);
    }
}

bool StartupSettings::ApplyChoice(bool enabled) {
    if (!SetRunEntry(enabled)) {
        return false;
    }

    const auto path = GetSettingsPath();
    if (path.empty()) {
        return false;
    }
    std::ofstream file(path, std::ios::trunc);
    if (!file) {
        return false;
    }
    file << (enabled ? "enabled\n" : "disabled\n");
    if (!file.good()) {
        return false;
    }

    answered_ = true;
    enabled_ = enabled;
    return true;
}

bool StartupSettings::IsAnswered() const noexcept {
    return answered_;
}

bool StartupSettings::IsEnabled() const noexcept {
    return enabled_;
}
