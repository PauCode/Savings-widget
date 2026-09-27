#include "PresetDepositButtons.h"

#include <string>

bool PresetDepositButtons::Create(HWND parent, HFONT font) {
    constexpr int kLeft = 20;
    constexpr int kTop = 194;
    constexpr int kWidth = 52;
    constexpr int kHeight = 28;
    constexpr int kGap = 5;

    for (std::size_t index = 0; index < kAmountsRon.size(); ++index) {
        const std::wstring label = std::to_wstring(kAmountsRon[index]);
        HWND button = CreateWindowW(
            L"BUTTON", label.c_str(), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            kLeft + static_cast<int>(index) * (kWidth + kGap),
            kTop, kWidth, kHeight, parent,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kFirstControlId + index)),
            nullptr, nullptr);
        if (!button) {
            return false;
        }

        SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }

    return true;
}

bool PresetDepositButtons::GetAmountForClick(
    WPARAM command, int& amountRon) const noexcept {
    if (HIWORD(command) != BN_CLICKED) {
        return false;
    }

    const int controlId = LOWORD(command);
    const int index = controlId - kFirstControlId;
    if (index < 0 || index >= static_cast<int>(kAmountsRon.size())) {
        return false;
    }

    amountRon = kAmountsRon[static_cast<std::size_t>(index)];
    return true;
}