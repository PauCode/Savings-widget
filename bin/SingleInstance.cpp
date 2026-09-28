#include "SingleInstance.h"

namespace {
constexpr wchar_t kMutexName[] = L"Local\\PauCode.SavingsJar.SingleInstance";
constexpr wchar_t kShowMessageName[] = L"PauCode.SavingsJar.ShowWindow";
} // namespace

UINT GetShowExistingInstanceMessage() {
    static const UINT message = RegisterWindowMessageW(kShowMessageName);
    return message;
}

SingleInstanceGuard::~SingleInstanceGuard() {
    if (mutex_) {
        ReleaseMutex(mutex_);
        CloseHandle(mutex_);
    }
}

bool SingleInstanceGuard::Acquire() {
    mutex_ = CreateMutexW(nullptr, TRUE, kMutexName);
    if (!mutex_) {
        return true;
    }

    if (GetLastError() != ERROR_ALREADY_EXISTS) {
        return true;
    }

    const UINT message = GetShowExistingInstanceMessage();
    if (message != 0) {
        PostMessageW(HWND_BROADCAST, message, 0, 0);
    }

    CloseHandle(mutex_);
    mutex_ = nullptr;
    return false;
}
