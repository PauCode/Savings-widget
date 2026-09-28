#pragma once

#include <string>

enum class CloseBehavior {
    Ask,
    Close,
    Minimize
};

class CloseBehaviorSettings {
public:
    void Load();
    bool Save() const;

    CloseBehavior Get() const noexcept;
    void Set(CloseBehavior behavior) noexcept;

private:
    CloseBehavior behavior_ = CloseBehavior::Ask;
};
