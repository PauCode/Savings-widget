#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <windows.h>
#include <shellapi.h>
#include <WebView2.h>
#include <wrl.h>

#include "../bin/CurrencyRates.h"
#include "../bin/CloseBehaviorSettings.h"
#include "../bin/GoalManager.h"
#include "../bin/ReminderSettings.h"
#include "../bin/StartupSettings.h"

class WebViewWindow {
public:
    WebViewWindow() = default;
    ~WebViewWindow();

    WebViewWindow(const WebViewWindow&) = delete;
    WebViewWindow& operator=(const WebViewWindow&) = delete;

    bool Run(HINSTANCE instance, int showCommand, int& exitCode);

private:
    struct CallbackLifetime {
        std::atomic<WebViewWindow*> owner{nullptr};
    };

    struct RatesFetchState {
        std::mutex mutex;
        bool completed = false;
        bool succeeded = false;
        CurrencyRates rates;
    };

    static constexpr UINT kFallbackMessage = WM_APP + 10;
    static constexpr UINT kRatesLoadedMessage = WM_APP + 11;
    static constexpr UINT kTrayIconMessage = WM_APP + 12;
    static constexpr UINT kTrayIconId = 1;
    static constexpr UINT kTrayMenuRestoreId = 1;
    static constexpr UINT kTrayMenuExitId = 2;
    static constexpr UINT_PTR kReminderTimerId = 1;
    static constexpr UINT kReminderIntervalMs = 60000;

    static LRESULT CALLBACK WindowProc(
        HWND window, UINT message, WPARAM wParam, LPARAM lParam);

    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void CreateWebView();
    void HandleWebViewCreated(HRESULT result, ICoreWebView2Environment* environment);
    void HandleControllerCreated(HRESULT result, ICoreWebView2Controller* controller);
    void HandleWebMessage(ICoreWebView2WebMessageReceivedEventArgs* args);
    void HandleRatesLoaded();
    void HandleDeposit(double amount);
    void HandleWithdrawal(double amount);
    void HandleReset();
    void HandleGoalTargetChange(double amount);
    void HandleGoalSelect(const std::wstring& id);
    void HandleGoalCreate(const std::wstring& name, double target);
    void HandleGoalRename(const std::wstring& id, const std::wstring& name);
    void HandleGoalDelete(const std::wstring& id);
    void HandleGoalArchive(const std::wstring& id);
    void HandleCloseRequest();
    void HandleCloseBehaviorSelection(const std::wstring& action, bool remember);
    void HandleStartupChoice(bool enabled);
    void HandleReminderUpdate(
        bool enabled, int dayOfMonth, bool useDefaultMessage,
        const std::wstring& message);
    void StartReminderTimer();
    void CheckReminderDue();
    void ShowReminderNotification(const std::wstring& message);
    void HandleThemeSelection(const std::wstring& filename);
    void HandleCurrencySelection(const std::wstring& code);
    void HandleRefreshRates();
    void FinalizeDefaultGoalIfNeeded();
    void StartRatesFetch();
    void SendState(const std::wstring& status = {});
    void RequestFallback();
    void ResizeWebView();
    void MigrateLegacyBalanceIfNeeded();
    std::optional<double> ConvertFromDisplayCurrency(double amount) const;
    std::optional<double> ConvertToDisplayCurrency(double amountRon) const;
    std::vector<std::wstring> FindThemes() const;
    void LoadSelectedTheme();
    void SaveSelectedTheme() const;
    void LoadSelectedCurrency();
    void SaveSelectedCurrency() const;
    void AddTrayIcon();
    void RemoveTrayIcon();
    void ShowTrayContextMenu();
    void RestoreFromTray();
    void ApplyModernTitleBar();

    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr;
    NOTIFYICONDATAW trayIcon_{};
    bool trayIconAdded_ = false;
    Microsoft::WRL::ComPtr<ICoreWebView2Environment> environment_;
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller_;
    Microsoft::WRL::ComPtr<ICoreWebView2> webView_;
    EventRegistrationToken processFailedToken_{};
    EventRegistrationToken webMessageToken_{};
    EventRegistrationToken navigationCompletedToken_{};
    std::shared_ptr<CallbackLifetime> callbackLifetime_;
    std::shared_ptr<RatesFetchState> ratesFetchState_;
    GoalManager goalManager_;
    CurrencyRates currencyRates_;
    CloseBehaviorSettings closeBehaviorSettings_;
    StartupSettings startupSettings_;
    ReminderSettings reminderSettings_;
    std::vector<std::wstring> themes_;
    std::wstring selectedTheme_ = L"default";
    std::wstring selectedCurrency_ = L"USD";
    std::wstring status_;
    bool ratesLoading_ = true;
    bool ratesAvailable_ = false;
    bool ratesRefreshing_ = false;
    bool pageReady_ = false;
    bool fallbackRequested_ = false;
    bool closePromptOpen_ = false;
};