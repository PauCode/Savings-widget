#define UNICODE
#define _UNICODE

#include "MoneySaverWindow.h"

#include <cmath>
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
constexpr int kAmountEditId = 1001;
constexpr int kAddModeControlId = 1003;
constexpr int kRemoveModeControlId = 1004;
constexpr int kResetControlId = 1005;

std::wstring FormatMoney(double amount, const wchar_t* currencyCode) {
    std::wostringstream text;
    text << std::fixed << std::setprecision(2) << amount
         << L" " << currencyCode;
    return text.str();
}
} // namespace

int NativeFallbackWindow::Run(HINSTANCE instance, int showCommand) {
    if (!goalManager_.Load()) {
        MessageBoxW(nullptr, L"Could not load savings data.",
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
    RECT bounds{0, 0, 320, 370};
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

LRESULT CALLBACK NativeFallbackWindow::WindowProc(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    NativeFallbackWindow* application = reinterpret_cast<NativeFallbackWindow*>(
        GetWindowLongPtrW(window, GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        const auto* createInfo = reinterpret_cast<CREATESTRUCTW*>(lParam);
        application = static_cast<NativeFallbackWindow*>(createInfo->lpCreateParams);
        application->window_ = window;
        SetWindowLongPtrW(
            window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(application));
    }

    if (!application) {
        return DefWindowProcW(window, message, wParam, lParam);
    }
    return application->HandleMessage(message, wParam, lParam);
}

LRESULT NativeFallbackWindow::HandleMessage(
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
            if (LOWORD(wParam) == kAddModeControlId &&
                HIWORD(wParam) == BN_CLICKED) {
                SetMode(true);
                return 0;
            }
            if (LOWORD(wParam) == kRemoveModeControlId &&
                HIWORD(wParam) == BN_CLICKED) {
                SetMode(false);
                return 0;
            }
            if (LOWORD(wParam) == kResetControlId &&
                HIWORD(wParam) == BN_CLICKED) {
                ResetSavings();
                return 0;
            }

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

bool NativeFallbackWindow::CreateControls() {
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

    HWND modeLabel = CreateWindowW(
        L"STATIC", L"Change savings:", WS_CHILD | WS_VISIBLE,
        20, 108, 96, 22, window_, nullptr, nullptr, nullptr);
    const DWORD radioStyle = WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON;
    addModeButton_ = CreateWindowW(
        L"BUTTON", L"Add", radioStyle | WS_GROUP,
        120, 106, 68, 24, window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kAddModeControlId)),
        nullptr, nullptr);
    removeModeButton_ = CreateWindowW(
        L"BUTTON", L"Remove", radioStyle,
        192, 106, 92, 24, window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kRemoveModeControlId)),
        nullptr, nullptr);
    SendMessageW(addModeButton_, BM_SETCHECK, BST_CHECKED, 0);

    HWND amountLabel = CreateWindowW(
        L"STATIC", L"Amount (RON):", WS_CHILD | WS_VISIBLE,
        20, 136, 160, 18, window_, nullptr, nullptr, nullptr);
    amountEdit_ = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"25.00",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        20, 156, 170, 26, window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kAmountEditId)),
        nullptr, nullptr);
    presetLabel_ = CreateWindowW(
        L"STATIC", L"Quick add (RON):", WS_CHILD | WS_VISIBLE,
        20, 190, 280, 20, window_, nullptr, nullptr, nullptr);
    HWND resetButton = CreateWindowW(
        L"BUTTON", L"Reset savings", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        20, 292, 120, 26, window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kResetControlId)),
        nullptr, nullptr);
    statusLabel_ = CreateWindowW(
        L"STATIC", L"", WS_CHILD | WS_VISIBLE,
        20, 326, 280, 32, window_, nullptr, nullptr, nullptr);

    if (!modeLabel || !addModeButton_ || !removeModeButton_ || !amountLabel ||
        !amountEdit_ || !statusLabel_ || !presetLabel_ || !resetButton ||
        !depositButton_.Create(window_, font_) ||
        !presetDepositButtons_.Create(window_, font_)) {
        return false;
    }

    for (HWND control : {savedLabel_, goalLabel_, modeLabel, addModeButton_,
                         removeModeButton_, amountLabel, amountEdit_,
                         presetLabel_, resetButton, statusLabel_}) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    }
    return true;
}

void NativeFallbackWindow::StartRatesFetch() {
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

void NativeFallbackWindow::HandleRatesLoaded() {
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
    FinalizeDefaultGoalIfNeeded();
    UpdateDisplay();
}

void NativeFallbackWindow::FinalizeDefaultGoalIfNeeded() {
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

void NativeFallbackWindow::UpdateDisplay() {
    const int savedCents = goalManager_.GetActiveSavedCents();
    const int targetRonCents = goalManager_.GetActiveGoalTargetRonCents();
    progress_.Update(savedCents, targetRonCents > 0 ? targetRonCents : 1);

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
    if (!savedRon || targetRonCents <= 0) {
        SetWindowTextW(savedLabel_, L"Saved: conversion unavailable");
        SetWindowTextW(goalLabel_, L"Goal: conversion unavailable");
        SetWindowTextW(statusLabel_, L"Could not convert the current balance.");
        return;
    }

    const int savedRonCents = static_cast<int>(std::lround(*savedRon * 100.0));
    progress_.Update(savedRonCents, targetRonCents);

    SetWindowTextW(
        savedLabel_, (L"Saved: " + FormatMoney(*savedRon, L"RON")).c_str());
    SetWindowTextW(
        goalLabel_, (L"Goal: " + FormatMoney(
            targetRonCents / 100.0, L"RON")).c_str());
    SetWindowTextW(
        statusLabel_, savedRonCents >= targetRonCents
                          ? L"Goal reached. Great work!"
                          : isAdding_ ? L"Every deposit gets you closer."
                                      : L"Remove funds when you need them.");
}

void NativeFallbackWindow::AddDeposit() {
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

void NativeFallbackWindow::AddDeposit(double amountRon, bool clearAmountEdit) {
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

    const DepositResult result = isAdding_
        ? goalManager_.AddDeposit(*amountUsd, amountRon)
        : goalManager_.RemoveFunds(*amountUsd, amountRon);
    if (result == DepositResult::InvalidAmount) {
        SetWindowTextW(statusLabel_, L"Enter a valid amount greater than 0 RON.");
        return;
    }
    if (result == DepositResult::TooLarge) {
        SetWindowTextW(statusLabel_, L"That deposit is too large.");
        return;
    }
    if (result == DepositResult::InsufficientFunds) {
        SetWindowTextW(statusLabel_, L"Not enough savings to remove that amount.");
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
    SetWindowTextW(statusLabel_, isAdding_ ? L"Deposit added."
                                           : L"Amount removed.");
    if (clearAmountEdit) {
        SetFocus(amountEdit_);
    }
}

void NativeFallbackWindow::SetMode(bool isAdding) {
    isAdding_ = isAdding;
    SendMessageW(addModeButton_, BM_SETCHECK,
                 isAdding_ ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(removeModeButton_, BM_SETCHECK,
                 isAdding_ ? BST_UNCHECKED : BST_CHECKED, 0);
    depositButton_.SetCaption(isAdding_ ? L"Add" : L"Remove");
    SetWindowTextW(presetLabel_, isAdding_
                                     ? L"Quick add (RON):"
                                     : L"Quick remove (RON):");
}

void NativeFallbackWindow::ResetSavings() {
    const int answer = MessageBoxW(
        window_, L"Reset the savings balance to zero? This cannot be undone.",
        L"Reset savings", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
    if (answer != IDYES) {
        return;
    }

    if (!goalManager_.ResetActiveGoal()) {
        SetWindowTextW(statusLabel_, L"Could not reset savings.");
        return;
    }

    UpdateDisplay();
    SetWindowTextW(statusLabel_, L"Savings reset.");
}