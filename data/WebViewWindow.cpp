#define UNICODE
#define _UNICODE
#define _SILENCE_EXPERIMENTAL_COROUTINE_DEPRECATION_WARNINGS

#include "WebViewWindow.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <utility>

#include <winrt/Windows.Data.Json.h>

#include <dwmapi.h>

#include "../bin/AppPaths.h"
#include "Resource.h"

#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Dwmapi.lib")

namespace {
using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif
#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif
#ifndef DWMWA_TEXT_COLOR
#define DWMWA_TEXT_COLOR 36
#endif

constexpr wchar_t kWindowClass[] = L"SavingsJarWebViewWindow";
constexpr wchar_t kVirtualHost[] = L"savings-jar.local";

class WinRtApartment {
public:
    WinRtApartment() {
        winrt::init_apartment(winrt::apartment_type::single_threaded);
    }

    ~WinRtApartment() {
        winrt::uninit_apartment();
    }

    WinRtApartment(const WinRtApartment&) = delete;
    WinRtApartment& operator=(const WinRtApartment&) = delete;
};

std::wstring EscapeJson(const std::wstring& value) {
    std::wstring escaped;
    escaped.reserve(value.size() + 8);
    for (wchar_t character : value) {
        switch (character) {
        case L'"': escaped += L"\\\""; break;
        case L'\\': escaped += L"\\\\"; break;
        case L'\b': escaped += L"\\b"; break;
        case L'\f': escaped += L"\\f"; break;
        case L'\n': escaped += L"\\n"; break;
        case L'\r': escaped += L"\\r"; break;
        case L'\t': escaped += L"\\t"; break;
        default:
            if (character < 0x20) {
                escaped += L"\\u00";
                constexpr wchar_t digits[] = L"0123456789abcdef";
                escaped += digits[(character >> 4) & 0x0f];
                escaped += digits[character & 0x0f];
            } else {
                escaped += character;
            }
        }
    }
    return escaped;
}

bool IsValidThemeName(const std::wstring& name) {
    if (name.empty()) {
        return false;
    }
    return std::all_of(name.begin(), name.end(),
                       [](wchar_t character) {
                           return (character >= L'a' && character <= L'z') ||
                                  (character >= L'A' && character <= L'Z') ||
                                  (character >= L'0' && character <= L'9') ||
                                  character == L'-' || character == L'_';
                       });
}

std::wstring WidenAscii(std::string_view text) {
    return std::wstring(text.begin(), text.end());
}

std::string NarrowAscii(const std::wstring& text) {
    std::string narrow;
    narrow.reserve(text.size());
    for (wchar_t character : text) {
        narrow.push_back(static_cast<char>(
            std::toupper(static_cast<unsigned char>(character & 0xff))));
    }
    return narrow;
}
} // namespace

WebViewWindow::~WebViewWindow() {
    if (callbackLifetime_) {
        callbackLifetime_->owner.store(nullptr);
    }
    webView_.Reset();
    controller_.Reset();
    environment_.Reset();
}

bool WebViewWindow::Run(HINSTANCE instance, int showCommand, int& exitCode) {
    if (!goalManager_.Load()) {
        return false;
    }

    try {
        WinRtApartment apartment;
        instance_ = instance;
        callbackLifetime_ = std::make_shared<CallbackLifetime>();
        callbackLifetime_->owner.store(this);
        themes_ = FindThemes();
        if (themes_.empty()) {
            RequestFallback();
            return false;
        }
        LoadSelectedTheme();
        LoadSelectedCurrency();
        closeBehaviorSettings_.Load();
        startupSettings_.Load();

        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.hInstance = instance_;
        windowClass.lpfnWndProc = WindowProc;
        windowClass.lpszClassName = kWindowClass;
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        windowClass.hbrBackground = CreateSolidBrush(RGB(0x08, 0x13, 0x1c));
        windowClass.hIcon = LoadIconW(instance_, MAKEINTRESOURCEW(IDI_APPICON));
        windowClass.hIconSm = windowClass.hIcon;
        if (!RegisterClassExW(&windowClass) &&
            GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }

        constexpr DWORD style = WS_OVERLAPPEDWINDOW;
        RECT bounds{0, 0, 1280, 840};
        AdjustWindowRectEx(&bounds, style, FALSE, 0);
        window_ = CreateWindowExW(
            0, kWindowClass, L"Savings Jar",
            style, CW_USEDEFAULT, CW_USEDEFAULT,
            bounds.right - bounds.left, bounds.bottom - bounds.top,
            nullptr, nullptr, instance_, this);
        if (!window_) {
            return false;
        }

        ApplyModernTitleBar();
        StartRatesFetch();
        CreateWebView();
        ShowWindow(window_, showCommand);
        UpdateWindow(window_);
        AddTrayIcon();

        MSG message{};
        int messageResult = 0;
        while ((messageResult = GetMessageW(&message, nullptr, 0, 0)) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        const bool shouldFallback = fallbackRequested_ || messageResult == -1;
        if (shouldFallback) {
            return false;
        }

        exitCode = static_cast<int>(message.wParam);
        return true;
    } catch (const winrt::hresult_error&) {
        return false;
    } catch (const std::system_error&) {
        return false;
    }
}

LRESULT CALLBACK WebViewWindow::WindowProc(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* application = reinterpret_cast<WebViewWindow*>(
        GetWindowLongPtrW(window, GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        const auto* createInfo = reinterpret_cast<CREATESTRUCTW*>(lParam);
        application = static_cast<WebViewWindow*>(createInfo->lpCreateParams);
        application->window_ = window;
        SetWindowLongPtrW(
            window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(application));
    }

    if (!application) {
        return DefWindowProcW(window, message, wParam, lParam);
    }
    return application->HandleMessage(message, wParam, lParam);
}

LRESULT WebViewWindow::HandleMessage(
    UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_SIZE:
        ResizeWebView();
        return 0;

    case WM_CLOSE:
        HandleCloseRequest();
        return 0;

    case kTrayIconMessage:
        switch (LOWORD(lParam)) {
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
            RestoreFromTray();
            break;
        case WM_RBUTTONUP:
            ShowTrayContextMenu();
            break;
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case kTrayMenuRestoreId:
            RestoreFromTray();
            return 0;
        case kTrayMenuExitId:
            DestroyWindow(window_);
            return 0;
        }
        break;

    case kFallbackMessage:
        if (window_) {
            DestroyWindow(window_);
        }
        return 0;

    case kRatesLoadedMessage:
        HandleRatesLoaded();
        return 0;

    case WM_DESTROY:
        RemoveTrayIcon();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(window_, message, wParam, lParam);
}

void WebViewWindow::AddTrayIcon() {
    if (trayIconAdded_ || !window_) {
        return;
    }

    trayIcon_.cbSize = sizeof(trayIcon_);
    trayIcon_.hWnd = window_;
    trayIcon_.uID = kTrayIconId;
    trayIcon_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    trayIcon_.uCallbackMessage = kTrayIconMessage;
    trayIcon_.hIcon = LoadIconW(instance_, MAKEINTRESOURCEW(IDI_APPICON));
    wcscpy_s(trayIcon_.szTip, L"Savings Jar");

    trayIconAdded_ = Shell_NotifyIconW(NIM_ADD, &trayIcon_) != FALSE;
}

void WebViewWindow::RemoveTrayIcon() {
    if (!trayIconAdded_) {
        return;
    }
    Shell_NotifyIconW(NIM_DELETE, &trayIcon_);
    trayIconAdded_ = false;
}

void WebViewWindow::ShowTrayContextMenu() {
    if (!window_) {
        return;
    }

    HMENU menu = CreatePopupMenu();
    if (!menu) {
        return;
    }

    AppendMenuW(menu, MF_STRING, kTrayMenuRestoreId, L"Open Savings Jar");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kTrayMenuExitId, L"Exit");

    POINT cursor{};
    GetCursorPos(&cursor);
    SetForegroundWindow(window_);
    TrackPopupMenu(
        menu, TPM_RIGHTBUTTON, cursor.x, cursor.y, 0, window_, nullptr);
    PostMessageW(window_, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

void WebViewWindow::RestoreFromTray() {
    if (!window_) {
        return;
    }
    ShowWindow(window_, SW_RESTORE);
    SetForegroundWindow(window_);
}

void WebViewWindow::ApplyModernTitleBar() {
    if (!window_) {
        return;
    }

    const HICON icon = LoadIconW(instance_, MAKEINTRESOURCEW(IDI_APPICON));
    SendMessageW(window_, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
    SendMessageW(window_, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));

    const BOOL darkMode = TRUE;
    DwmSetWindowAttribute(
        window_, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));

    constexpr COLORREF kCaptionColor = RGB(0x11, 0x1d, 0x29);
    constexpr COLORREF kTextColor = RGB(0xef, 0xf5, 0xf2);
    constexpr COLORREF kBorderColor = RGB(0x29, 0x38, 0x44);
    DwmSetWindowAttribute(
        window_, DWMWA_CAPTION_COLOR, &kCaptionColor, sizeof(kCaptionColor));
    DwmSetWindowAttribute(
        window_, DWMWA_TEXT_COLOR, &kTextColor, sizeof(kTextColor));
    DwmSetWindowAttribute(
        window_, DWMWA_BORDER_COLOR, &kBorderColor, sizeof(kBorderColor));
}

void WebViewWindow::CreateWebView() {
    const auto dataDirectory = AppPaths::DataDirectory();
    if (dataDirectory.empty()) {
        RequestFallback();
        return;
    }

    const auto userDataPath = dataDirectory / L"webview2-profile";
    std::error_code error;
    std::filesystem::create_directories(userDataPath, error);
    if (error) {
        RequestFallback();
        return;
    }

    const std::weak_ptr<CallbackLifetime> weakLifetime = callbackLifetime_;
    const HRESULT result = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userDataPath.c_str(), nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [weakLifetime](HRESULT errorCode,
                           ICoreWebView2Environment* environment) -> HRESULT {
                const auto lifetime = weakLifetime.lock();
                auto* owner = lifetime ? lifetime->owner.load() : nullptr;
                if (owner) {
                    owner->HandleWebViewCreated(errorCode, environment);
                }
                return S_OK;
            }).Get());
    if (FAILED(result)) {
        RequestFallback();
    }
}

void WebViewWindow::HandleWebViewCreated(
    HRESULT result, ICoreWebView2Environment* environment) {
    if (FAILED(result) || !environment) {
        RequestFallback();
        return;
    }

    environment_ = environment;
    const std::weak_ptr<CallbackLifetime> weakLifetime = callbackLifetime_;
    const HRESULT controllerResult = environment_->CreateCoreWebView2Controller(
        window_,
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [weakLifetime](HRESULT errorCode,
                           ICoreWebView2Controller* controller) -> HRESULT {
                const auto lifetime = weakLifetime.lock();
                auto* owner = lifetime ? lifetime->owner.load() : nullptr;
                if (owner) {
                    owner->HandleControllerCreated(errorCode, controller);
                }
                return S_OK;
            }).Get());
    if (FAILED(controllerResult)) {
        RequestFallback();
    }
}

void WebViewWindow::HandleControllerCreated(
    HRESULT result, ICoreWebView2Controller* controller) {
    if (FAILED(result) || !controller) {
        RequestFallback();
        return;
    }

    controller_ = controller;
    if (FAILED(controller_->get_CoreWebView2(&webView_))) {
        RequestFallback();
        return;
    }

    ComPtr<ICoreWebView2Settings> settings;
    if (SUCCEEDED(webView_->get_Settings(&settings))) {
        settings->put_IsStatusBarEnabled(FALSE);
        settings->put_AreDefaultContextMenusEnabled(FALSE);
        settings->put_IsZoomControlEnabled(FALSE);
    }

    const auto assetDirectory = AppPaths::ExecutableDirectory();
    ComPtr<ICoreWebView2_3> webView3;
    if (assetDirectory.empty() || FAILED(webView_.As(&webView3)) ||
        FAILED(webView3->SetVirtualHostNameToFolderMapping(
            kVirtualHost, assetDirectory.c_str(),
            COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS))) {
        RequestFallback();
        return;
    }

    const std::weak_ptr<CallbackLifetime> weakLifetime = callbackLifetime_;
    if (FAILED(webView_->add_ProcessFailed(
            Callback<ICoreWebView2ProcessFailedEventHandler>(
                [weakLifetime](ICoreWebView2*,
                               ICoreWebView2ProcessFailedEventArgs*) -> HRESULT {
                    const auto lifetime = weakLifetime.lock();
                    auto* owner = lifetime ? lifetime->owner.load() : nullptr;
                    if (owner) {
                        owner->RequestFallback();
                    }
                    return S_OK;
                }).Get(), &processFailedToken_)) ||
        FAILED(webView_->add_WebMessageReceived(
            Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                [weakLifetime](ICoreWebView2*,
                               ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                    const auto lifetime = weakLifetime.lock();
                    auto* owner = lifetime ? lifetime->owner.load() : nullptr;
                    if (owner) {
                        owner->HandleWebMessage(args);
                    }
                    return S_OK;
                }).Get(), &webMessageToken_)) ||
        FAILED(webView_->add_NavigationCompleted(
            Callback<ICoreWebView2NavigationCompletedEventHandler>(
                [weakLifetime](ICoreWebView2*,
                               ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                    const auto lifetime = weakLifetime.lock();
                    auto* owner = lifetime ? lifetime->owner.load() : nullptr;
                    if (owner) {
                        BOOL succeeded = FALSE;
                        if (FAILED(args->get_IsSuccess(&succeeded)) || !succeeded) {
                            owner->RequestFallback();
                        } else {
                            owner->pageReady_ = true;
                            owner->SendState();
                        }
                    }
                    return S_OK;
                }).Get(), &navigationCompletedToken_))) {
        RequestFallback();
        return;
    }

    ResizeWebView();
    controller_->put_IsVisible(TRUE);
    const std::wstring url =
        std::wstring(L"https://") + kVirtualHost + L"/ui/index.html";
    if (FAILED(webView_->Navigate(url.c_str()))) {
        RequestFallback();
    }
}

void WebViewWindow::HandleWebMessage(
    ICoreWebView2WebMessageReceivedEventArgs* args) {
    LPWSTR rawJson = nullptr;
    if (!args || FAILED(args->get_WebMessageAsJson(&rawJson)) || !rawJson) {
        return;
    }

    const std::wstring json(rawJson);
    CoTaskMemFree(rawJson);

    try {
        const auto message = winrt::Windows::Data::Json::JsonObject::Parse(
            winrt::hstring(json));
        const auto type = message.GetNamedString(L"type");
        if (type == L"ready") {
            pageReady_ = true;
            SendState();
        } else if (type == L"deposit") {
            HandleDeposit(message.GetNamedNumber(L"amount"));
        } else if (type == L"withdraw") {
            HandleWithdrawal(message.GetNamedNumber(L"amount"));
        } else if (type == L"reset") {
            HandleReset();
        } else if (type == L"goal") {
            HandleGoalTargetChange(message.GetNamedNumber(L"amount"));
        } else if (type == L"selectGoal") {
            HandleGoalSelect(std::wstring(message.GetNamedString(L"id")));
        } else if (type == L"createGoal") {
            HandleGoalCreate(
                std::wstring(message.GetNamedString(L"name")),
                message.GetNamedNumber(L"target"));
        } else if (type == L"renameGoal") {
            HandleGoalRename(
                std::wstring(message.GetNamedString(L"id")),
                std::wstring(message.GetNamedString(L"name")));
        } else if (type == L"deleteGoal") {
            HandleGoalDelete(std::wstring(message.GetNamedString(L"id")));
        } else if (type == L"archiveGoal") {
            HandleGoalArchive(std::wstring(message.GetNamedString(L"id")));
        } else if (type == L"closeBehavior") {
            HandleCloseBehaviorSelection(
                std::wstring(message.GetNamedString(L"action")),
                message.GetNamedBoolean(L"remember", false));
        } else if (type == L"startupChoice") {
            HandleStartupChoice(message.GetNamedBoolean(L"enabled", false));
        } else if (type == L"theme") {
            HandleThemeSelection(
                std::wstring(message.GetNamedString(L"file")));
        } else if (type == L"currency") {
            HandleCurrencySelection(
                std::wstring(message.GetNamedString(L"code")));
        } else if (type == L"refreshRates") {
            HandleRefreshRates();
        }
    } catch (const winrt::hresult_error&) {
    }
}

void WebViewWindow::StartRatesFetch() {
    ratesFetchState_ = std::make_shared<RatesFetchState>();
    const auto state = ratesFetchState_;
    const HWND window = window_;
    try {
        std::thread([state, window] {
            CurrencyRates rates;
            const bool succeeded = rates.FetchLatestRates();
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                state->succeeded = succeeded;
                if (succeeded) {
                    state->rates = std::move(rates);
                }
                state->completed = true;
            }
            PostMessageW(window, kRatesLoadedMessage, 0, 0);
        }).detach();
    } catch (const std::system_error&) {
        ratesLoading_ = false;
        ratesRefreshing_ = false;
        SendState(L"Could not start the exchange-rate request.");
    }
}

void WebViewWindow::HandleRatesLoaded() {
    if (!ratesFetchState_) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(ratesFetchState_->mutex);
        if (!ratesFetchState_->completed) {
            return;
        }
        ratesAvailable_ = ratesFetchState_->succeeded;
        if (ratesAvailable_) {
            currencyRates_ = std::move(ratesFetchState_->rates);
        }
    }
    ratesLoading_ = false;
    ratesRefreshing_ = false;
    MigrateLegacyBalanceIfNeeded();
    FinalizeDefaultGoalIfNeeded();
    SendState();
}

void WebViewWindow::MigrateLegacyBalanceIfNeeded() {
    if (!ratesAvailable_ || !goalManager_.ActiveGoalNeedsLegacyUnitMigration()) {
        return;
    }

    const auto convertedRon = currencyRates_.Convert(
        goalManager_.GetActiveSavedCents() / 100.0, "USD", "RON");
    goalManager_.MigrateActiveGoalBalanceToRon(convertedRon.value_or(0.0));
}

void WebViewWindow::FinalizeDefaultGoalIfNeeded() {
    if (!ratesAvailable_ || goalManager_.GetActiveGoalTargetRonCents() > 0) {
        return;
    }

    const auto defaultGoalRon = currencyRates_.Convert(1000.0, "USD", "RON");
    if (!defaultGoalRon || *defaultGoalRon <= 0.0) {
        return;
    }

    goalManager_.SetActiveGoalTargetRonCents(
        static_cast<int>(std::lround(*defaultGoalRon * 100.0)));
}

void WebViewWindow::HandleDeposit(double amount) {
    if (!ratesAvailable_) {
        SendState(ratesLoading_ ? L"Exchange rates are still loading."
                                : L"Exchange rates are unavailable.");
        return;
    }

    const auto amountRon = ConvertFromDisplayCurrency(amount);
    if (!amountRon) {
        SendState(L"Enter a valid amount greater than 0.");
        return;
    }

    const DepositResult result = goalManager_.AddDeposit(*amountRon);
    switch (result) {
    case DepositResult::InvalidAmount:
        SendState(L"Enter a valid amount greater than 0.");
        return;
    case DepositResult::TooLarge:
        SendState(L"That deposit is too large.");
        return;
    case DepositResult::SaveFailed:
        SendState(L"Could not save data.");
        return;
    case DepositResult::Added:
        SendState(L"Deposit added.");
        return;
    case DepositResult::Removed:
    case DepositResult::InsufficientFunds:
        break;
    }
}

void WebViewWindow::HandleWithdrawal(double amount) {
    if (!ratesAvailable_) {
        SendState(ratesLoading_ ? L"Exchange rates are still loading."
                                : L"Exchange rates are unavailable.");
        return;
    }

    const auto amountRon = ConvertFromDisplayCurrency(amount);
    if (!amountRon) {
        SendState(L"Enter a valid amount greater than 0.");
        return;
    }

    const DepositResult result = goalManager_.RemoveFunds(*amountRon);
    switch (result) {
    case DepositResult::Removed:
        SendState(L"Amount removed.");
        return;
    case DepositResult::InsufficientFunds:
        SendState(L"Not enough savings to remove that amount.");
        return;
    case DepositResult::InvalidAmount:
        SendState(L"Enter a valid amount greater than 0.");
        return;
    case DepositResult::TooLarge:
        SendState(L"That amount is too large.");
        return;
    case DepositResult::SaveFailed:
        SendState(L"Could not save data.");
        return;
    case DepositResult::Added:
        return;
    }
}

void WebViewWindow::HandleReset() {
    const int answer = MessageBoxW(
        window_, L"Reset the savings balance to zero? This cannot be undone.",
        L"Reset savings", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
    if (answer != IDYES) {
        return;
    }

    if (!goalManager_.ResetActiveGoal()) {
        SendState(L"Could not reset savings.");
        return;
    }

    SendState(L"Savings reset.");
}

void WebViewWindow::HandleGoalTargetChange(double amount) {
    if (!ratesAvailable_) {
        SendState(L"Exchange rates are unavailable; the goal was not changed.");
        return;
    }

    const auto amountRon = ConvertFromDisplayCurrency(amount);
    if (!amountRon) {
        SendState(L"Enter a valid goal greater than 0.");
        return;
    }

    const long targetRonCents = std::lround(*amountRon * 100.0);
    if (targetRonCents <= 0 ||
        targetRonCents > SavingsData::kMaximumSavedCents) {
        SendState(L"That goal is too large.");
        return;
    }

    if (!goalManager_.SetActiveGoalTargetRonCents(static_cast<int>(targetRonCents))) {
        SendState(L"Could not save the new goal.");
        return;
    }

    SendState(L"Savings goal updated.");
}

void WebViewWindow::HandleGoalSelect(const std::wstring& id) {
    if (!goalManager_.SelectGoal(id)) {
        SendState(L"Could not switch goals.");
        return;
    }
    MigrateLegacyBalanceIfNeeded();
    SendState();
}

void WebViewWindow::HandleGoalCreate(const std::wstring& name, double target) {
    if (!ratesAvailable_) {
        SendState(L"Exchange rates are unavailable; try again shortly.");
        return;
    }

    const auto targetRon = ConvertFromDisplayCurrency(target);
    if (!targetRon) {
        SendState(L"Enter a valid target greater than 0.");
        return;
    }

    const long targetRonCents = std::lround(*targetRon * 100.0);
    if (targetRonCents <= 0 ||
        targetRonCents > SavingsData::kMaximumSavedCents) {
        SendState(L"Enter a valid target greater than 0.");
        return;
    }
    if (name.empty()) {
        SendState(L"Enter a name for the new jar.");
        return;
    }

    std::wstring newId;
    if (!goalManager_.CreateGoal(name, static_cast<int>(targetRonCents), newId)) {
        SendState(L"Could not create the new jar.");
        return;
    }

    SendState(L"New jar created.");
}

void WebViewWindow::HandleGoalDelete(const std::wstring& id) {
    if (!goalManager_.DeleteGoal(id)) {
        SendState(L"Could not delete the jar. Keep at least one active jar.");
        return;
    }
    MigrateLegacyBalanceIfNeeded();
    SendState(L"Jar deleted.");
}

void WebViewWindow::HandleGoalRename(
    const std::wstring& id, const std::wstring& name) {
    if (!goalManager_.RenameGoal(id, name)) {
        SendState(L"Enter a valid jar name.");
        return;
    }
    SendState(L"Jar renamed.");
}

void WebViewWindow::HandleGoalArchive(const std::wstring& id) {
    if (!goalManager_.ArchiveGoal(id)) {
        SendState(L"Only completed jars can be archived, and one active jar must remain.");
        return;
    }
    MigrateLegacyBalanceIfNeeded();
    SendState(L"Completed jar archived.");
}

void WebViewWindow::HandleCloseRequest() {
    switch (closeBehaviorSettings_.Get()) {
    case CloseBehavior::Close:
        DestroyWindow(window_);
        return;
    case CloseBehavior::Minimize:
        ShowWindow(window_, SW_MINIMIZE);
        return;
    case CloseBehavior::Ask:
        break;
    }

    if (closePromptOpen_) {
        return;
    }
    if (!webView_ || !pageReady_) {
        ShowWindow(window_, SW_MINIMIZE);
        return;
    }
    closePromptOpen_ = true;
    webView_->PostWebMessageAsJson(L"{\"type\":\"closeRequested\"}");
}

void WebViewWindow::HandleCloseBehaviorSelection(
    const std::wstring& action, bool remember) {
    closePromptOpen_ = false;
    const CloseBehavior selected = action == L"close"
        ? CloseBehavior::Close
        : CloseBehavior::Minimize;

    if (remember) {
        closeBehaviorSettings_.Set(selected);
        closeBehaviorSettings_.Save();
    }

    if (selected == CloseBehavior::Close) {
        DestroyWindow(window_);
    } else {
        ShowWindow(window_, SW_MINIMIZE);
    }
}

void WebViewWindow::HandleStartupChoice(bool enabled) {
    if (!startupSettings_.ApplyChoice(enabled)) {
        SendState(L"Could not save the Windows startup preference.");
        return;
    }
    SendState(enabled
        ? L"Savings Jar will start with Windows."
        : L"Windows startup was left disabled.");
}

void WebViewWindow::HandleThemeSelection(const std::wstring& filename) {
    if (std::find(themes_.begin(), themes_.end(), filename) == themes_.end()) {
        return;
    }
    selectedTheme_ = filename;
    SaveSelectedTheme();
    SendState();
}

void WebViewWindow::HandleCurrencySelection(const std::wstring& code) {
    const auto& supported = CurrencyRates::SupportedCurrencies();
    const std::string narrowCode = NarrowAscii(code);
    if (std::find(supported.begin(), supported.end(), narrowCode) == supported.end()) {
        return;
    }
    selectedCurrency_ = WidenAscii(narrowCode);
    SaveSelectedCurrency();
    SendState();
}

void WebViewWindow::HandleRefreshRates() {
    if (ratesRefreshing_) {
        return;
    }
    ratesRefreshing_ = true;
    SendState(L"Refreshing exchange rates...");
    StartRatesFetch();
}

std::optional<double> WebViewWindow::ConvertFromDisplayCurrency(double amount) const {
    if (!std::isfinite(amount) || amount <= 0.0) {
        return std::nullopt;
    }
    const std::string code = NarrowAscii(selectedCurrency_);
    if (code == "RON") {
        return amount;
    }
    return currencyRates_.Convert(amount, code, "RON");
}

std::optional<double> WebViewWindow::ConvertToDisplayCurrency(double amountRon) const {
    const std::string code = NarrowAscii(selectedCurrency_);
    if (code == "RON") {
        return amountRon;
    }
    return currencyRates_.Convert(amountRon, "RON", code);
}

void WebViewWindow::SendState(const std::wstring& status) {
    if (!webView_ || !pageReady_) {
        return;
    }

    const int savedCents = goalManager_.GetActiveSavedCents();
    const int targetRonCents = goalManager_.GetActiveGoalTargetRonCents();
    const double goalRon = targetRonCents > 0 ? targetRonCents / 100.0 : 0.0;
    const auto balanceRon = ratesAvailable_
        ? std::optional<double>{savedCents / 100.0}
        : std::optional<double>{};
    const auto balanceDisplay = balanceRon
        ? ConvertToDisplayCurrency(*balanceRon)
        : std::optional<double>{};
    const auto goalDisplay = (ratesAvailable_ && goalRon > 0.0)
        ? ConvertToDisplayCurrency(goalRon)
        : std::optional<double>{};
    const double remainingRon = balanceRon
        ? (std::max)(0.0, goalRon - *balanceRon)
        : 0.0;
    const auto remainingDisplay = ratesAvailable_
        ? ConvertToDisplayCurrency(remainingRon)
        : std::optional<double>{};

    if (!status.empty()) {
        status_ = status;
    } else if (ratesAvailable_ && balanceRon) {
        status_ = (goalRon > 0.0 && *balanceRon >= goalRon)
            ? L"Goal reached. Great work!"
            : L"Every deposit gets you closer.";
    } else if (ratesLoading_) {
        status_ = L"Connecting to exchange rates...";
    } else {
        status_ = L"Exchange rates are unavailable. Deposits are disabled.";
    }

    const double rawProgressRatio = (balanceRon && goalRon > 0.0)
        ? *balanceRon * 100.0 / goalRon
        : 0.0;
    const double progressRatio = rawProgressRatio > 0.0 ? rawProgressRatio : 0.0;
    const double progressPercent = std::clamp(progressRatio, 0.0, 100.0);

    std::wostringstream json;
    json.imbue(std::locale::classic());
    json << std::fixed << std::setprecision(2)
         << L"{\"type\":\"state\",\"ratesAvailable\":"
         << (ratesAvailable_ ? L"true" : L"false")
         << L",\"ratesLoading\":" << (ratesLoading_ ? L"true" : L"false")
         << L",\"ratesRefreshing\":" << (ratesRefreshing_ ? L"true" : L"false")
         << L",\"startupChoiceRequired\":"
         << (!startupSettings_.IsAnswered() ? L"true" : L"false")
         << L",\"balanceRon\":" << balanceDisplay.value_or(0.0)
         << L",\"goalRon\":" << goalDisplay.value_or(0.0)
         << L",\"remainingRon\":" << remainingDisplay.value_or(0.0)
         << L",\"progressPercent\":" << progressPercent
         << L",\"progressRatio\":" << progressRatio
         << L",\"rateDate\":\""
         << EscapeJson(ratesAvailable_
                           ? std::wstring(currencyRates_.GetRateDate().begin(),
                                          currencyRates_.GetRateDate().end())
                           : std::wstring{})
         << L"\",\"status\":\"" << EscapeJson(status_)
         << L"\",\"selectedTheme\":\"" << EscapeJson(selectedTheme_)
         << L"\",\"currency\":\"" << EscapeJson(selectedCurrency_)
         << L"\",\"activeGoalId\":\"" << EscapeJson(goalManager_.GetActiveGoalId())
         << L"\",\"activeGoalName\":\"" << EscapeJson(goalManager_.GetActiveGoalName())
         << L"\",\"isActiveGoalComplete\":"
         << ((targetRonCents > 0 && savedCents >= targetRonCents) ? L"true" : L"false")
         << L",\"canArchiveActiveGoal\":"
         << ((targetRonCents > 0 && savedCents >= targetRonCents &&
              goalManager_.GetGoals().size() > 1) ? L"true" : L"false")
         << L",\"currencies\":[";

    const auto& supportedCurrencies = CurrencyRates::SupportedCurrencies();
    for (std::size_t index = 0; index < supportedCurrencies.size(); ++index) {
        if (index > 0) {
            json << L',';
        }
        json << L'"' << WidenAscii(supportedCurrencies[index]) << L'"';
    }
    json << L"],\"goals\":[";

    const auto& goals = goalManager_.GetGoals();
    for (std::size_t index = 0; index < goals.size(); ++index) {
        if (index > 0) {
            json << L',';
        }
        json << L"{\"id\":\"" << EscapeJson(goals[index].id)
             << L"\",\"name\":\"" << EscapeJson(goals[index].name) << L"\"}";
    }
    json << L"],\"archivedGoals\":[";

    const auto& archivedGoals = goalManager_.GetArchivedGoals();
    for (std::size_t index = 0; index < archivedGoals.size(); ++index) {
        if (index > 0) {
            json << L',';
        }
        json << L"{\"id\":\"" << EscapeJson(archivedGoals[index].id)
             << L"\",\"name\":\"" << EscapeJson(archivedGoals[index].name)
             << L"\"}";
    }
    json << L"],\"history\":[";

    const auto history = goalManager_.GetActiveHistory(15);
    for (std::size_t index = 0; index < history.size(); ++index) {
        if (index > 0) {
            json << L',';
        }
        const double amountRon = history[index].amountRonCents / 100.0;
        const double amountDisplay = (history[index].type == L"reset")
            ? 0.0
            : ConvertToDisplayCurrency(amountRon).value_or(amountRon);
        json << L"{\"timestamp\":" << history[index].timestampMillis
             << L",\"type\":\"" << EscapeJson(history[index].type)
             << L"\",\"amountRon\":" << amountDisplay
             << L"}";
    }
    json << L"],\"themes\":[";

    for (std::size_t index = 0; index < themes_.size(); ++index) {
        if (index > 0) {
            json << L',';
        }
        json << L'"' << EscapeJson(themes_[index]) << L'"';
    }
    json << L"]}";
    webView_->PostWebMessageAsJson(json.str().c_str());
}

void WebViewWindow::RequestFallback() {
    if (fallbackRequested_) {
        return;
    }
    fallbackRequested_ = true;
    if (window_) {
        PostMessageW(window_, kFallbackMessage, 0, 0);
    }
}

void WebViewWindow::ResizeWebView() {
    if (!controller_ || !window_) {
        return;
    }
    RECT bounds{};
    GetClientRect(window_, &bounds);
    controller_->put_Bounds(bounds);
}

std::vector<std::wstring> WebViewWindow::FindThemes() const {
    std::vector<std::wstring> themes;
    const auto assetDirectory = AppPaths::ExecutableDirectory();
    if (assetDirectory.empty()) {
        return themes;
    }

    std::error_code error;
    const auto themeDirectory = assetDirectory / L"Theme";
    for (std::filesystem::directory_iterator iterator(themeDirectory, error), end;
         !error && iterator != end; iterator.increment(error)) {
        if (!iterator->is_directory(error) || error) {
            continue;
        }
        const std::wstring name = iterator->path().filename().wstring();
        if (!IsValidThemeName(name)) {
            continue;
        }
        std::error_code cssError;
        const auto cssPath = iterator->path() / (name + L".css");
        if (std::filesystem::is_regular_file(cssPath, cssError) && !cssError) {
            themes.push_back(name);
        }
    }

    std::sort(themes.begin(), themes.end());
    return themes;
}

void WebViewWindow::LoadSelectedTheme() {
    selectedTheme_ = L"default";
    const auto dataDirectory = AppPaths::DataDirectory();
    if (dataDirectory.empty()) {
        return;
    }

    std::ifstream file(dataDirectory / L"theme.txt");
    std::string selected;
    if (!file || !std::getline(file, selected)) {
        return;
    }

    const std::wstring candidate(selected.begin(), selected.end());
    if (std::find(themes_.begin(), themes_.end(), candidate) != themes_.end()) {
        selectedTheme_ = candidate;
    }
}

void WebViewWindow::SaveSelectedTheme() const {
    const auto dataDirectory = AppPaths::DataDirectory();
    if (dataDirectory.empty()) {
        return;
    }

    std::ofstream file(dataDirectory / L"theme.txt", std::ios::trunc);
    if (file) {
        std::string filename;
        filename.reserve(selectedTheme_.size());
        for (wchar_t character : selectedTheme_) {
            filename.push_back(static_cast<char>(character));
        }
        file << filename << '\n';
    }
}

void WebViewWindow::LoadSelectedCurrency() {
    selectedCurrency_ = L"USD";
    const auto dataDirectory = AppPaths::DataDirectory();
    if (dataDirectory.empty()) {
        return;
    }

    std::ifstream file(dataDirectory / L"currency.txt");
    std::string selected;
    if (!file || !std::getline(file, selected)) {
        return;
    }

    for (char& character : selected) {
        character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    }

    const auto& supported = CurrencyRates::SupportedCurrencies();
    if (std::find(supported.begin(), supported.end(), selected) != supported.end()) {
        selectedCurrency_ = WidenAscii(selected);
    }
}

void WebViewWindow::SaveSelectedCurrency() const {
    const auto dataDirectory = AppPaths::DataDirectory();
    if (dataDirectory.empty()) {
        return;
    }

    std::ofstream file(dataDirectory / L"currency.txt", std::ios::trunc);
    if (file) {
        file << NarrowAscii(selectedCurrency_) << '\n';
    }
}