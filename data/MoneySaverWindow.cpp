#define UNICODE
#define _UNICODE

#include "MoneySaverWindow.h"

#include <cwchar>
#include <cwctype>
#include <iomanip>
#include <iterator>
#include <sstream>

#include <commctrl.h>

#pragma comment(lib, "Comctl32.lib")

namespace {
constexpr wchar_t kWindowClass[] = L"MoneySavingWidgetWindow";
constexpr int kGoalCents = 100000;
constexpr int kAmountEditId = 1001;

std::wstring FormatMoney(int cents) {
    std::wostringstream text;
    text << L"$" << cents / 100 << L"." << std::setw(2)
         << std::setfill(L'0') << cents % 100;
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
    RECT bounds{0, 0, 320, 220};
    AdjustWindowRectEx(&bounds, style, FALSE, extendedStyle);

    window_ = CreateWindowExW(
        extendedStyle, kWindowClass, L"Money Saver",
        style, CW_USEDEFAULT, CW_USEDEFAULT,
        bounds.right - bounds.left, bounds.bottom - bounds.top,
        nullptr, nullptr, instance_, this);
    if (!window_) {
        return 1;
    }

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
        if (depositButton_.WasClicked(wParam)) {
            AddDeposit();
            return 0;
        }
        break;

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
    HWND goalLabel = CreateWindowW(
        L"STATIC", L"Goal: $1,000.00", WS_CHILD | WS_VISIBLE,
        20, 48, 280, 20, window_, nullptr, nullptr, nullptr);

    if (!savedLabel_ || !goalLabel || !progress_.Create(window_, font_)) {
        return false;
    }

    HWND depositLabel = CreateWindowW(
        L"STATIC", L"Add a deposit:", WS_CHILD | WS_VISIBLE,
        20, 108, 100, 20, window_, nullptr, nullptr, nullptr);
    amountEdit_ = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"25.00",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        20, 132, 170, 26, window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kAmountEditId)),
        nullptr, nullptr);
    statusLabel_ = CreateWindowW(
        L"STATIC", L"", WS_CHILD | WS_VISIBLE,
        20, 170, 280, 24, window_, nullptr, nullptr, nullptr);

    if (!depositLabel || !amountEdit_ || !statusLabel_ ||
        !depositButton_.Create(window_, font_)) {
        return false;
    }

    for (HWND control : {savedLabel_, amountEdit_, statusLabel_}) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    }
    return true;
}

void MoneySaverWindow::UpdateDisplay() {
    const int savedCents = savingsData_.GetSavedCents();
    SetWindowTextW(
        savedLabel_, (L"Saved: " + FormatMoney(savedCents)).c_str());
    progress_.Update(savedCents, kGoalCents);
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
        SetWindowTextW(statusLabel_, L"Enter a valid amount greater than $0.");
        return;
    }

    const DepositResult result = savingsData_.AddDeposit(amount);
    if (result == DepositResult::InvalidAmount) {
        SetWindowTextW(statusLabel_, L"Enter a valid amount greater than $0.");
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

    SetWindowTextW(amountEdit_, L"");
    UpdateDisplay();
    SetFocus(amountEdit_);
}