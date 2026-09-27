#pragma once

#include <windows.h>

class DepositButton {
public:
    bool Create(HWND parent, HFONT font);
    bool WasClicked(WPARAM command) const noexcept;

private:
    static constexpr int kControlId = 1002;
};