#include "ReminderSettings.h"
#include "AppPaths.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

#include <windows.h>

namespace {
constexpr wchar_t kDefaultMessage[] =
    L"The date has come for you to save some more money, deposit now to "
    L"continue towards your goals!";
constexpr std::size_t kMaximumMessageLength = 180;

std::filesystem::path GetSettingsPath() {
    const auto dataDirectory = AppPaths::DataDirectory();
    return dataDirectory.empty()
        ? std::filesystem::path{}
        : dataDirectory / L"reminder.txt";
}

std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(
        CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (length <= 0) {
        return {};
    }
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(
        CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), length);
    return wide;
}

std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }
    const int length = WideCharToMultiByte(
        CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0,
        nullptr, nullptr);
    if (length <= 0) {
        return {};
    }
    std::string narrow(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, text.data(), static_cast<int>(text.size()), narrow.data(),
        length, nullptr, nullptr);
    return narrow;
}

std::wstring SanitizeMessage(const std::wstring& rawMessage) {
    std::wstring message;
    message.reserve(rawMessage.size());
    for (wchar_t character : rawMessage) {
        message.push_back(
            character == L'\n' || character == L'\r' || character == L'\t'
                ? L' '
                : character);
    }

    const auto isSpace = [](wchar_t character) { return character == L' '; };
    while (!message.empty() && isSpace(message.front())) {
        message.erase(message.begin());
    }
    while (!message.empty() && isSpace(message.back())) {
        message.pop_back();
    }
    if (message.size() > kMaximumMessageLength) {
        message.resize(kMaximumMessageLength);
    }
    return message;
}
} // namespace

const wchar_t* ReminderSettings::DefaultMessage() noexcept {
    return kDefaultMessage;
}

void ReminderSettings::Load() {
    enabled_ = false;
    dayOfMonth_ = 1;
    useDefaultMessage_ = true;
    customMessage_.clear();
    lastShownStamp_.clear();

    std::ifstream file(GetSettingsPath());
    if (!file) {
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }
        const std::string key = line.substr(0, separator);
        const std::string value = line.substr(separator + 1);

        if (key == "enabled") {
            enabled_ = value == "1";
        } else if (key == "day") {
            try {
                dayOfMonth_ = std::clamp(std::stoi(value), 1, 31);
            } catch (const std::exception&) {
                dayOfMonth_ = 1;
            }
        } else if (key == "default") {
            useDefaultMessage_ = value != "0";
        } else if (key == "message") {
            customMessage_ = SanitizeMessage(Utf8ToWide(value));
        } else if (key == "lastShown") {
            lastShownStamp_ = Utf8ToWide(value);
        }
    }
}

bool ReminderSettings::IsEnabled() const noexcept {
    return enabled_;
}

int ReminderSettings::GetDayOfMonth() const noexcept {
    return dayOfMonth_;
}

bool ReminderSettings::UsesDefaultMessage() const noexcept {
    return useDefaultMessage_;
}

const std::wstring& ReminderSettings::GetCustomMessage() const noexcept {
    return customMessage_;
}

std::wstring ReminderSettings::ResolveMessage() const {
    if (useDefaultMessage_ || customMessage_.empty()) {
        return kDefaultMessage;
    }
    return customMessage_;
}

bool ReminderSettings::Configure(
    bool enabled, int dayOfMonth, bool useDefaultMessage,
    const std::wstring& message) {
    if (dayOfMonth < 1 || dayOfMonth > 31) {
        return false;
    }

    const std::wstring sanitized = SanitizeMessage(message);
    if (!useDefaultMessage && sanitized.empty()) {
        return false;
    }

    enabled_ = enabled;
    dayOfMonth_ = dayOfMonth;
    useDefaultMessage_ = useDefaultMessage;
    customMessage_ = useDefaultMessage ? std::wstring{} : sanitized;
    return Save();
}

bool ReminderSettings::IsDueFor(
    const std::wstring& monthStamp, int dayOfMonth) const {
    return enabled_ && dayOfMonth >= dayOfMonth_ && lastShownStamp_ != monthStamp;
}

bool ReminderSettings::MarkShown(const std::wstring& monthStamp) {
    lastShownStamp_ = monthStamp;
    return Save();
}

bool ReminderSettings::Save() const {
    const auto path = GetSettingsPath();
    if (path.empty()) {
        return false;
    }

    std::ofstream file(path, std::ios::trunc);
    if (!file) {
        return false;
    }

    file << "enabled=" << (enabled_ ? '1' : '0') << '\n'
         << "day=" << dayOfMonth_ << '\n'
         << "default=" << (useDefaultMessage_ ? '1' : '0') << '\n'
         << "message=" << WideToUtf8(customMessage_) << '\n'
         << "lastShown=" << WideToUtf8(lastShownStamp_) << '\n';
    return file.good();
}
