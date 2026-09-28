#include "AppPaths.h"

#include <system_error>

#include <windows.h>
#include <shlobj.h>

namespace AppPaths {
std::filesystem::path ExecutableDirectory() {
    wchar_t executablePath[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, executablePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return {};
    }
    return std::filesystem::path(executablePath).parent_path();
}

std::filesystem::path DataDirectory() {
    PWSTR roamingPath = nullptr;
    if (FAILED(SHGetKnownFolderPath(
            FOLDERID_RoamingAppData, KF_FLAG_CREATE, nullptr, &roamingPath))) {
        return {};
    }

    const std::filesystem::path directory =
        std::filesystem::path(roamingPath) / L"PauCode" / L"Savings Jar";
    CoTaskMemFree(roamingPath);

    std::error_code error;
    std::filesystem::create_directories(directory, error);
    return error ? std::filesystem::path{} : directory;
}

} // namespace AppPaths
