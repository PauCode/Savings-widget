#define UNICODE
#define _UNICODE
#define _SILENCE_EXPERIMENTAL_COROUTINE_DEPRECATION_WARNINGS

#include "WebViewWindow.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <utility>

#include <winrt/Windows.Data.Json.h>

#pragma comment(lib, "Ole32.lib")

namespace {
using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

constexpr wchar_t kWindowClass[] = L"SavingsJarWebViewWindow";
constexpr wchar_t kVirtualHost[] = L"savings-jar.local";
constexpr int kGoalCents = 100000;

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

std::filesystem::path GetExecutablePath() {
    wchar_t executablePath[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(
        nullptr, executablePath, static_cast<DWORD>(std::size(executablePath)));
    if (length == 0 || length >= std::size(executablePath)) {
        return {};
    }
    return std::filesystem::path(executablePath);
}

std::filesystem::path GetWorkspaceRoot() {
    const auto executablePath = GetExecutablePath();
    if (executablePath.empty()) {
        return {};
    }
    return executablePath.parent_path().parent_path().parent_path();
}

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

bool IsValidThemeFilename(const std::wstring& filename) {
    if (filename.size() < 5 || filename.substr(filename.size() - 4) != L".css") {
        return false;
    }
    return std::all_of(filename.begin(), filename.end() - 4,
                       [](wchar_t character) {
                           return (character >= L'a' && character <= L'z') ||
                                  (character >= L'A' && character <= L'Z') ||
                                  (character >= L'0' && character <= L'9') ||
                                  character == L'-' || character == L'_';
                       });
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
    if (!savingsData_.Load()) {
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

        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.hInstance = instance_;
        windowClass.lpfnWndProc = WindowProc;
        windowClass.lpszClassName = kWindowClass;
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        if (!RegisterClassExW(&windowClass) &&
            GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }

        constexpr DWORD style = WS_OVERLAPPEDWINDOW;
        RECT bounds{0, 0, 1120, 760};
        AdjustWindowRectEx(&bounds, style, FALSE, 0);
        window_ = CreateWindowExW(
            0, kWindowClass, L"Savings Jar",
            style, CW_USEDEFAULT, CW_USEDEFAULT,
            bounds.right - bounds.left, bounds.bottom - bounds.top,
            nullptr, nullptr, instance_, this);
        if (!window_) {
            return false;
        }

        StartRatesFetch();
        CreateWebView();
        ShowWindow(window_, showCommand);
        UpdateWindow(window_);

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

    case kFallbackMessage:
        if (window_) {
            DestroyWindow(window_);
        }
        return 0;

    case kRatesLoadedMessage:
        HandleRatesLoaded();
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(window_, message, wParam, lParam);
}

void WebViewWindow::CreateWebView() {
    const auto workspaceRoot = GetWorkspaceRoot();
    if (workspaceRoot.empty()) {
        RequestFallback();
        return;
    }

    const auto userDataPath = workspaceRoot / L"current" / L"webview2-profile";
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

    const auto workspaceRoot = GetWorkspaceRoot();
    ComPtr<ICoreWebView2_3> webView3;
    if (workspaceRoot.empty() || FAILED(webView_.As(&webView3)) ||
        FAILED(webView3->SetVirtualHostNameToFolderMapping(
            kVirtualHost, workspaceRoot.c_str(),
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
        std::wstring(L"https://") + kVirtualHost + L"/data/ui/index.html";
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
            HandleDeposit(message.GetNamedNumber(L"amountRon"));
        } else if (type == L"theme") {
            HandleThemeSelection(
                std::wstring(message.GetNamedString(L"file")));
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
    SendState();
}

void WebViewWindow::HandleDeposit(double amountRon) {
    if (!ratesAvailable_) {
        SendState(ratesLoading_ ? L"RON rates are still loading."
                                : L"RON rates are unavailable.");
        return;
    }

    const auto amountUsd = currencyRates_.Convert(amountRon, "RON", "USD");
    if (!amountUsd || *amountUsd <= 0.0) {
        SendState(L"Enter a valid amount greater than 0 RON.");
        return;
    }

    const DepositResult result = savingsData_.AddDeposit(*amountUsd);
    switch (result) {
    case DepositResult::InvalidAmount:
        SendState(L"Enter a valid amount greater than 0 RON.");
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
    }
}

void WebViewWindow::HandleThemeSelection(const std::wstring& filename) {
    if (std::find(themes_.begin(), themes_.end(), filename) == themes_.end()) {
        return;
    }
    selectedTheme_ = filename;
    SaveSelectedTheme();
    SendState();
}

void WebViewWindow::SendState(const std::wstring& status) {
    if (!webView_ || !pageReady_) {
        return;
    }

    if (!status.empty()) {
        status_ = status;
    } else if (ratesAvailable_) {
        status_ = savingsData_.GetSavedCents() >= kGoalCents
            ? L"Goal reached. Great work!"
            : L"Every deposit gets you closer.";
    } else if (ratesLoading_) {
        status_ = L"Connecting to exchange rates...";
    } else {
        status_ = L"RON rates are unavailable. Deposits are disabled.";
    }

    const int savedCents = savingsData_.GetSavedCents();
    const auto balanceRon = ratesAvailable_
        ? currencyRates_.Convert(savedCents / 100.0, "USD", "RON")
        : std::optional<double>{};
    const auto goalRon = ratesAvailable_
        ? currencyRates_.Convert(kGoalCents / 100.0, "USD", "RON")
        : std::optional<double>{};
    const double progressPercent = std::clamp(
        static_cast<double>(savedCents) * 100.0 / kGoalCents, 0.0, 100.0);

    std::wostringstream json;
    json.imbue(std::locale::classic());
    json << std::fixed << std::setprecision(2)
         << L"{\"type\":\"state\",\"ratesAvailable\":"
         << (ratesAvailable_ ? L"true" : L"false")
         << L",\"ratesLoading\":" << (ratesLoading_ ? L"true" : L"false")
         << L",\"balanceRon\":" << balanceRon.value_or(0.0)
         << L",\"goalRon\":" << goalRon.value_or(0.0)
         << L",\"progressPercent\":" << progressPercent
         << L",\"rateDate\":\""
         << EscapeJson(ratesAvailable_
                           ? std::wstring(currencyRates_.GetRateDate().begin(),
                                          currencyRates_.GetRateDate().end())
                           : std::wstring{})
         << L"\",\"status\":\"" << EscapeJson(status_)
         << L"\",\"selectedTheme\":\"" << EscapeJson(selectedTheme_)
         << L"\",\"themes\":[";

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
    const auto workspaceRoot = GetWorkspaceRoot();
    if (workspaceRoot.empty()) {
        return themes;
    }

    std::error_code error;
    const auto themeDirectory = workspaceRoot / L"Theme";
    for (std::filesystem::directory_iterator iterator(themeDirectory, error), end;
         !error && iterator != end; iterator.increment(error)) {
        if (!iterator->is_regular_file(error) || error) {
            continue;
        }
        const std::wstring filename = iterator->path().filename().wstring();
        if (IsValidThemeFilename(filename)) {
            themes.push_back(filename);
        }
    }

    std::sort(themes.begin(), themes.end());
    return themes;
}

void WebViewWindow::LoadSelectedTheme() {
    selectedTheme_ = L"default.css";
    const auto workspaceRoot = GetWorkspaceRoot();
    if (workspaceRoot.empty()) {
        return;
    }

    std::ifstream file(workspaceRoot / L"current" / L"theme.txt");
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
    const auto workspaceRoot = GetWorkspaceRoot();
    if (workspaceRoot.empty()) {
        return;
    }

    std::error_code error;
    const auto currentDirectory = workspaceRoot / L"current";
    std::filesystem::create_directories(currentDirectory, error);
    if (error) {
        return;
    }

    std::ofstream file(currentDirectory / L"theme.txt", std::ios::trunc);
    if (file) {
        std::string filename;
        filename.reserve(selectedTheme_.size());
        for (wchar_t character : selectedTheme_) {
            filename.push_back(static_cast<char>(character));
        }
        file << filename << '\n';
    }
}