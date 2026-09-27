#pragma once

#include <memory>
#include <mutex>
#include <thread>
#include <windows.h>

#include "../bin/CurrencyRates.h"
#include "DepositButton.h"
#include "../bin/GoalManager.h"
#include "PresetDepositButtons.h"
#include "SavingsProgress.h"

class NativeFallbackWindow {
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
    void SetMode(bool isAdding);
    void ResetSavings();
    void FinalizeDefaultGoalIfNeeded();
    void MigrateLegacyBalanceIfNeeded();

    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr;
    HWND savedLabel_ = nullptr;
    HWND goalLabel_ = nullptr;
    HWND addModeButton_ = nullptr;
    HWND removeModeButton_ = nullptr;
    HWND amountEdit_ = nullptr;
    HWND presetLabel_ = nullptr;
    HWND statusLabel_ = nullptr;
    HFONT font_ = nullptr;
    bool isAdding_ = true;
    bool ratesLoading_ = true;
    bool ratesAvailable_ = false;
    GoalManager goalManager_;
    CurrencyRates currencyRates_;
    std::shared_ptr<RatesFetchState> ratesFetchState_;
    DepositButton depositButton_;
    PresetDepositButtons presetDepositButtons_;
    SavingsProgress progress_;
};