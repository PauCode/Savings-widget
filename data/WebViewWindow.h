#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <windows.h>
#include <WebView2.h>
#include <wrl.h>

#include "../bin/CurrencyRates.h"
#include "../bin/SavingsData.h"

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

    static LRESULT CALLBACK WindowProc(
        HWND window, UINT message, WPARAM wParam, LPARAM lParam);

    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void CreateWebView();
    void HandleWebViewCreated(HRESULT result, ICoreWebView2Environment* environment);
    void HandleControllerCreated(HRESULT result, ICoreWebView2Controller* controller);
    void HandleWebMessage(ICoreWebView2WebMessageReceivedEventArgs* args);
    void HandleRatesLoaded();
    void HandleDeposit(double amountRon);
    void HandleWithdrawal(double amountRon);
    void HandleReset();
    void HandleThemeSelection(const std::wstring& filename);
    void StartRatesFetch();
    void SendState(const std::wstring& status = {});
    void RequestFallback();
    void ResizeWebView();
    std::vector<std::wstring> FindThemes() const;
    void LoadSelectedTheme();
    void SaveSelectedTheme() const;

    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr;
    Microsoft::WRL::ComPtr<ICoreWebView2Environment> environment_;
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller_;
    Microsoft::WRL::ComPtr<ICoreWebView2> webView_;
    EventRegistrationToken processFailedToken_{};
    EventRegistrationToken webMessageToken_{};
    EventRegistrationToken navigationCompletedToken_{};
    std::shared_ptr<CallbackLifetime> callbackLifetime_;
    std::shared_ptr<RatesFetchState> ratesFetchState_;
    SavingsData savingsData_;
    CurrencyRates currencyRates_;
    std::vector<std::wstring> themes_;
    std::wstring selectedTheme_ = L"default.css";
    std::wstring status_;
    bool ratesLoading_ = true;
    bool ratesAvailable_ = false;
    bool pageReady_ = false;
    bool fallbackRequested_ = false;
};