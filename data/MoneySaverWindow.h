#pragma once

#include <windows.h>

#include "DepositButton.h"
#include "../bin/SavingsData.h"
#include "SavingsProgress.h"

class MoneySaverWindow {
public:
    int Run(HINSTANCE instance, int showCommand);

private:
    static LRESULT CALLBACK WindowProc(
        HWND window, UINT message, WPARAM wParam, LPARAM lParam);

    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    bool CreateControls();
    void UpdateDisplay();
    void AddDeposit();

    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr;
    HWND savedLabel_ = nullptr;
    HWND amountEdit_ = nullptr;
    HWND statusLabel_ = nullptr;
    HFONT font_ = nullptr;
    SavingsData savingsData_;
    DepositButton depositButton_;
    SavingsProgress progress_;
};