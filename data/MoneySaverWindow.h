#pragma once

#include <memory>
#include <mutex>
#include <thread>
#include <windows.h>

#include "../bin/CurrencyRates.h"
#include "DepositButton.h"
#include "../bin/SavingsData.h"
#include "PresetDepositButtons.h"
#include "SavingsProgress.h"

class MoneySaverWindow {
public:
    int Run(HINSTANCE instance, int showCommand);

private:
    struct RatesFetchState {
        std::mutex mutex;
        bool completed = false;
        bool succeeded = false;
        CurrencyRates rates;
    };

    static constexpr UINT kRatesLoadedMessage = WM_APP + 1;

    static LRESULT CALLBACK WindowProc(
        HWND window, UINT message, WPARAM wParam, LPARAM lParam);

    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    bool CreateControls();
    void StartRatesFetch();
    void HandleRatesLoaded();
    void UpdateDisplay();
    void AddDeposit();
    void AddDeposit(double amountRon, bool clearAmountEdit);

    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr;
    HWND savedLabel_ = nullptr;
    HWND goalLabel_ = nullptr;
    HWND amountEdit_ = nullptr;
    HWND statusLabel_ = nullptr;
    HFONT font_ = nullptr;
    bool ratesLoading_ = true;
    bool ratesAvailable_ = false;
    SavingsData savingsData_;
    CurrencyRates currencyRates_;
    std::shared_ptr<RatesFetchState> ratesFetchState_;
    DepositButton depositButton_;
    PresetDepositButtons presetDepositButtons_;
    SavingsProgress progress_;
};