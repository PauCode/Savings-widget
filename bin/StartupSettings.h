#pragma once

class StartupSettings {
public:
    void Load();
    bool ApplyChoice(bool enabled);

    bool IsAnswered() const noexcept;
    bool IsEnabled() const noexcept;

private:
    bool answered_ = false;
    bool enabled_ = false;
};
