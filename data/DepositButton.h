#pragma once

#include <windows.h>

class DepositButton {
public:
    bool Create(HWND parent, HFONT font);
    bool WasClicked(WPARAM command) const noexcept;
    void SetCaption(const wchar_t* caption) const;

private:
    static constexpr int kControlId = 1002;
    HWND button_ = nullptr;
};