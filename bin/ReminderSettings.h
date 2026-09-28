#pragma once

#include <string>

// Stores the optional monthly savings reminder in
// %APPDATA%/PauCode/Savings Jar/reminder.txt. The reminder fires once per
// calendar month on the configured day; lastShown tracks the "YYYY-MM" that
// was already notified so a restart cannot repeat it.
class ReminderSettings {
public:
    static const wchar_t* DefaultMessage() noexcept;

    void Load();

    bool IsEnabled() const noexcept;
    int GetDayOfMonth() const noexcept;
    bool UsesDefaultMessage() const noexcept;
    const std::wstring& GetCustomMessage() const noexcept;
    std::wstring ResolveMessage() const;

    bool Configure(
        bool enabled, int dayOfMonth, bool useDefaultMessage,
        const std::wstring& message);
    bool IsDueFor(const std::wstring& monthStamp, int dayOfMonth) const;
    bool MarkShown(const std::wstring& monthStamp);

private:
    bool Save() const;

    bool enabled_ = false;
    int dayOfMonth_ = 1;
    bool useDefaultMessage_ = true;
    std::wstring customMessage_;
    std::wstring lastShownStamp_;
};
