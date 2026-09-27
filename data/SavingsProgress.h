#pragma once

#include <windows.h>

class SavingsProgress {
public:
    bool Create(HWND parent, HFONT font);
    void Update(int savedCents, int goalCents) const;

private:
    HWND progressBar_ = nullptr;
    HWND percentageLabel_ = nullptr;
};