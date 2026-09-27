#pragma once

#include <array>
#include <windows.h>

class PresetDepositButtons {
public:
    bool Create(HWND parent, HFONT font);
    bool GetAmountForClick(WPARAM command, int& amountRon) const noexcept;

private:
    static constexpr int kFirstControlId = 1100;
    static constexpr std::array<int, 6> kAmountsRon{
        50, 100, 250, 500, 1000, 1500
    };
};