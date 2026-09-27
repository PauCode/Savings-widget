#define UNICODE
#define _UNICODE

#include "MoneySaverWindow.h"

#include <cwchar>
#include <cwctype>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <utility>

#include <commctrl.h>

#pragma comment(lib, "Comctl32.lib")

namespace {
constexpr wchar_t kWindowClass[] = L"MoneySavingWidgetWindow";
constexpr int kGoalCents = 100000;
constexpr int kAmountEditId = 1001;

std::wstring FormatMoney(double amount, const wchar_t* currencyCode) {
    std::wostringstream text;
    text << std::fixed << std::setprecision(2) << amount
         << L" " << currencyCode;
    return text.str();
}
} // namespace

int MoneySaverWindow::Run(HINSTANCE instance, int showCommand) {
    if (!savingsData_.Load()) {
        MessageBoxW(nullptr, L"Could not load current\\savings.dat.",
                    L"Money Saver", MB_OK | MB_ICONERROR);
        return 1;
    }

    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_PROGRESS_CLASS};
    InitCommonControlsEx(&controls);

    instance_ = instance;
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.hInstance = instance_;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.lpszClassName = kWindowClass;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    if (!RegisterClassExW(&windowClass)) {
        return 1;
    }

    constexpr DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    constexpr DWORD extendedStyle = WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
    RECT bounds{0, 0, 320, 270};
    AdjustWindowRectEx(&bounds, style, FALSE, extendedStyle);

    window_ = CreateWindowExW(
        extendedStyle, kWindowClass, L"Money Saver",
        style, CW_USEDEFAULT, CW_USEDEFAULT,
        bounds.right - bounds.left, bounds.bottom - bounds.top,
        nullptr, nullptr, instance_, this);
    if (!window_) {
        return 1;
    }

    StartRatesFetch();
    ShowWindow(window_, showCommand);
    UpdateWindow(window_);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}

LRESULT CALLBACK MoneySaverWindow::WindowProc(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    MoneySaverWindow* application = reinterpret_cast<MoneySaverWindow*>(
        GetWindowLongPtrW(window, GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        const auto* createInfo = reinterpret_cast<CREATESTRUCTW*>(lParam);
        application = static_cast<MoneySaverWindow*>(createInfo->lpCreateParams);
        application->window_ = window;
        SetWindowLongPtrW(
            window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(application));
    }

    if (!application) {
        return DefWindowProcW(window, message, wParam, lParam);
    }
    return application->HandleMessage(message, wParam, lParam);
}

LRESULT MoneySaverWindow::HandleMessage(
    UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        if (!CreateControls()) {
            return -1;
        }
        UpdateDisplay();
        return 0;

    case WM_COMMAND:
        {
            int presetAmountRon = 0;
            if (presetDepositButtons_.GetAmountForClick(wParam, presetAmountRon)) {
                AddDeposit(static_cast<double>(presetAmountRon), false);
                return 0;
            }
        }
        if (depositButton_.WasClicked(wParam)) {
            AddDeposit();
            return 0;
        }
        break;

    case kRatesLoadedMessage:
        HandleRatesLoaded();
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(window_, message, wParam, lParam);
}

bool MoneySaverWindow::CreateControls() {
    font_ = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

    savedLabel_ = CreateWindowW(
        L"STATIC", L"", WS_CHILD | WS_VISIBLE,
        20, 18, 280, 24, window_, nullptr, nullptr, nullptr);
    goalLabel_ = CreateWindowW(
        L"STATIC", L"", WS_CHILD | WS_VISIBLE,
        20, 48, 280, 20, window_, nullptr, nullptr, nullptr);

    if (!savedLabel_ || !goalLabel_ || !progress_.Create(window_, font_)) {
        return false;
    }

    HWND depositLabel = CreateWindowW(
        L"STATIC", L"Add a deposit (RON):", WS_CHILD | WS_VISIBLE,
        20, 108, 160, 20, window_, nullptr, nullptr, nullptr);
    amountEdit_ = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"25.00",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        20, 132, 170, 26, window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kAmountEditId)),
        nullptr, nullptr);
    statusLabel_ = CreateWindowW(
        L"STATIC", L"", WS_CHILD | WS_VISIBLE,
        20, 230, 280, 28, window_, nullptr, nullptr, nullptr);
    HWND presetLabel = CreateWindowW(
        L"STATIC", L"Quick add (RON):", WS_CHILD | WS_VISIBLE,
        20, 168, 280, 20, window_, nullptr, nullptr, nullptr);

    if (!depositLabel || !amountEdit_ || !statusLabel_ || !presetLabel ||
        !depositButton_.Create(window_, font_) ||
        !presetDepositButtons_.Create(window_, font_)) {
        return false;
    }

    for (HWND control : {savedLabel_, goalLabel_, amountEdit_,
                         statusLabel_, presetLabel}) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    }
    return true;
}

void MoneySaverWindow::StartRatesFetch() {
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
        UpdateDisplay();
    }
}

void MoneySaverWindow::HandleRatesLoaded() {
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
    UpdateDisplay();
}

void MoneySaverWindow::UpdateDisplay() {
    const int savedCents = savingsData_.GetSavedCents();
    progress_.Update(savedCents, kGoalCents);

    if (!ratesAvailable_) {
        SetWindowTextW(savedLabel_, L"Saved: waiting for RON rates");
        SetWindowTextW(goalLabel_, L"Goal: waiting for RON rates");
        SetWindowTextW(
            statusLabel_, ratesLoading_
                             ? L"Loading current RON rates..."
                             : L"RON rates unavailable. Deposits are disabled.");
        return;
    }

    const auto savedRon = currencyRates_.Convert(
        savedCents / 100.0, "USD", "RON");
    const auto goalRon = currencyRates_.Convert(
        kGoalCents / 100.0, "USD", "RON");
    if (!savedRon || !goalRon) {
        SetWindowTextW(savedLabel_, L"Saved: conversion unavailable");
        SetWindowTextW(goalLabel_, L"Goal: conversion unavailable");
        SetWindowTextW(statusLabel_, L"Could not convert the current balance.");
        return;
    }

    SetWindowTextW(
        savedLabel_, (L"Saved: " + FormatMoney(*savedRon, L"RON")).c_str());
    SetWindowTextW(
        goalLabel_, (L"Goal: " + FormatMoney(*goalRon, L"RON")).c_str());
    SetWindowTextW(
        statusLabel_, savedCents >= kGoalCents
                          ? L"Goal reached. Great work!"
                          : L"Every deposit gets you closer.");
}

void MoneySaverWindow::AddDeposit() {
    wchar_t buffer[64]{};
    GetWindowTextW(amountEdit_, buffer, static_cast<int>(std::size(buffer)));

    wchar_t* end = nullptr;
    const double amount = std::wcstod(buffer, &end);
    while (end && std::iswspace(*end)) {
        ++end;
    }

    if (end == buffer || (end && *end != L'\0')) {
        SetWindowTextW(statusLabel_, L"Enter a valid amount greater than 0 RON.");
        return;
    }

    AddDeposit(amount, true);
}

void MoneySaverWindow::AddDeposit(double amountRon, bool clearAmountEdit) {
    if (!ratesAvailable_) {
        SetWindowTextW(statusLabel_, ratesLoading_
                                         ? L"RON rates are still loading."
                                         : L"RON rates are unavailable.");
        return;
    }

    const auto amountUsd = currencyRates_.Convert(amountRon, "RON", "USD");
    if (!amountUsd || *amountUsd <= 0.0) {
        SetWindowTextW(statusLabel_, L"Enter a valid amount greater than 0 RON.");
        return;
    }

    const DepositResult result = savingsData_.AddDeposit(*amountUsd);
    if (result == DepositResult::InvalidAmount) {
        SetWindowTextW(statusLabel_, L"Enter a valid amount greater than 0 RON.");
        return;
    }
    if (result == DepositResult::TooLarge) {
        SetWindowTextW(statusLabel_, L"That deposit is too large.");
        return;
    }
    if (result == DepositResult::SaveFailed) {
        SetWindowTextW(statusLabel_, L"Could not save data.");
        return;
    }

    if (clearAmountEdit) {
        SetWindowTextW(amountEdit_, L"");
    }
    UpdateDisplay();
    if (clearAmountEdit) {
        SetFocus(amountEdit_);
    }
}