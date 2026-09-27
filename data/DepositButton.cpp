#include "DepositButton.h"

bool DepositButton::Create(HWND parent, HFONT font) {
    button_ = CreateWindowW(
        L"BUTTON", L"Add", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        200, 156, 100, 26, parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kControlId)),
        nullptr, nullptr);
    if (!button_) {
        return false;
    }

    SendMessageW(button_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    return true;
}

bool DepositButton::WasClicked(WPARAM command) const noexcept {
    return LOWORD(command) == kControlId && HIWORD(command) == BN_CLICKED;
}

void DepositButton::SetCaption(const wchar_t* caption) const {
    if (button_) {
        SetWindowTextW(button_, caption);
    }
}