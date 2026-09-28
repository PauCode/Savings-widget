#pragma once

#include <windows.h>

// Registered window message broadcast by a second launch so the running
// instance can reveal itself instead of starting a duplicate process.
UINT GetShowExistingInstanceMessage();

class SingleInstanceGuard {
public:
    SingleInstanceGuard() = default;
    ~SingleInstanceGuard();

    SingleInstanceGuard(const SingleInstanceGuard&) = delete;
    SingleInstanceGuard& operator=(const SingleInstanceGuard&) = delete;

    // Returns false when another instance already owns the lock; that
    // instance is asked to show its window before this one exits.
    bool Acquire();

private:
    HANDLE mutex_ = nullptr;
};
