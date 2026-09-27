#include "DepositButton.h"

bool DepositButton::Create(HWND parent, HFONT font) {
    HWND button = CreateWindowW(
        L"BUTTON", L"Add", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        200, 132, 100, 26, parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kControlId)),
        nullptr, nullptr);
    if (!button) {
        return false;
    }

    SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    return true;
}

bool DepositButton::WasClicked(WPARAM command) const noexcept {
    return LOWORD(command) == kControlId && HIWORD(command) == BN_CLICKED;
}