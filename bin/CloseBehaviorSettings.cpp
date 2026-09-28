#include "CloseBehaviorSettings.h"
#include "AppPaths.h"

#include <filesystem>
#include <fstream>
#include <system_error>

namespace {
std::filesystem::path GetSettingsPath() {
    const auto dataDirectory = AppPaths::DataDirectory();
    return dataDirectory.empty()
        ? std::filesystem::path{}
        : dataDirectory / L"close-behavior.txt";
}
} // namespace

void CloseBehaviorSettings::Load() {
    behavior_ = CloseBehavior::Ask;
    std::ifstream file(GetSettingsPath());
    std::string value;
    if (!file || !std::getline(file, value)) {
        return;
    }
    if (value == "close") {
        behavior_ = CloseBehavior::Close;
    } else if (value == "minimize") {
        behavior_ = CloseBehavior::Minimize;
    }
}

bool CloseBehaviorSettings::Save() const {
    const auto path = GetSettingsPath();
    if (path.empty()) {
        return false;
    }

    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        return false;
    }

    std::ofstream file(path, std::ios::trunc);
    if (!file) {
        return false;
    }
    switch (behavior_) {
    case CloseBehavior::Close:
        file << "close\n";
        break;
    case CloseBehavior::Minimize:
        file << "minimize\n";
        break;
    case CloseBehavior::Ask:
        file << "ask\n";
        break;
    }
    return file.good();
}

CloseBehavior CloseBehaviorSettings::Get() const noexcept {
    return behavior_;
}

void CloseBehaviorSettings::Set(CloseBehavior behavior) noexcept {
    behavior_ = behavior;
}
