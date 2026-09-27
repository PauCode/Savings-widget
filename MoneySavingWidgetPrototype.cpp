#define UNICODE
#define _UNICODE

#include <windows.h>
#include <commctrl.h>

#include <cwchar>
#include <iterator>
#include <algorithm>
#include <cmath>
#include <cwctype>
#include <iomanip>
#include <sstream>

#pragma comment(lib, "Comctl32.lib")

namespace {
constexpr wchar_t kWindowClass[] = L"MoneySavingWidgetWindow";
constexpr int kGoalCents = 100000; // $1,000.00
constexpr int kAmountEditId = 1001;
constexpr int kAddButtonId = 1002;

HWND g_amountEdit = nullptr;
HWND g_savedLabel = nullptr;
HWND g_progressBar = nullptr;
HWND g_statusLabel = nullptr;
int g_savedCents = 0;

std::wstring FormatMoney(int cents) {
    std::wostringstream text;
    text << L"$" << cents / 100 << L"." << std::setw(2)
         << std::setfill(L'0') << cents % 100;
    return text.str();
}

void UpdateWidget() {
    SetWindowTextW(g_savedLabel,
                   (L"Saved: " + FormatMoney(g_savedCents)).c_str());

    const int progress = std::clamp(
        static_cast<int>((static_cast<double>(g_savedCents) / kGoalCents) * 100),
        0, 100);
    SendMessageW(g_progressBar, PBM_SETPOS, progress, 0);

    SetWindowTextW(g_statusLabel,
                   g_savedCents >= kGoalCents
                       ? L"Goal reached. Great work!"
                       : L"Every deposit gets you closer.");
}

void AddDeposit(HWND window) {
    wchar_t buffer[64]{};
    GetWindowTextW(g_amountEdit, buffer, static_cast<int>(std::size(buffer)));

    wchar_t* end = nullptr;
    const double amount = std::wcstod(buffer, &end);

    while (end && std::iswspace(*end)) {
        ++end;
    }

    if (end == buffer || (end && *end != L'\0') ||
        !std::isfinite(amount) || amount <= 0.0 || amount > 1000000.0) {
        SetWindowTextW(g_statusLabel, L"Enter a valid amount greater than $0.");
        return;
    }

    const int depositCents = static_cast<int>(std::lround(amount * 100.0));
    if (depositCents <= 0 || depositCents > 100000000 - g_savedCents) {
        SetWindowTextW(g_statusLabel, L"That deposit is too large.");
        return;
    }

    g_savedCents += depositCents;
    SetWindowTextW(g_amountEdit, L"");
    UpdateWidget();
    SetFocus(g_amountEdit);
    (void)window;
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        const HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

        g_savedLabel = CreateWindowW(
            L"STATIC", L"", WS_CHILD | WS_VISIBLE,
            20, 18, 280, 24, window, nullptr, nullptr, nullptr);

        CreateWindowW(
            L"STATIC", L"Goal: $1,000.00", WS_CHILD | WS_VISIBLE,
            20, 48, 280, 20, window, nullptr, nullptr, nullptr);

        g_progressBar = CreateWindowExW(
            0, PROGRESS_CLASSW, nullptr, WS_CHILD | WS_VISIBLE,
            20, 76, 280, 18, window, nullptr, nullptr, nullptr);
        SendMessageW(g_progressBar, PBM_SETRANGE32, 0, 100);

        CreateWindowW(
            L"STATIC", L"Add a deposit:", WS_CHILD | WS_VISIBLE,
            20, 108, 100, 20, window, nullptr, nullptr, nullptr);

        g_amountEdit = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"EDIT", L"25.00",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            20, 132, 170, 26, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kAmountEditId)),
            nullptr, nullptr);

        CreateWindowW(
            L"BUTTON", L"Add", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            200, 132, 100, 26, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kAddButtonId)),
            nullptr, nullptr);

        g_statusLabel = CreateWindowW(
            L"STATIC", L"", WS_CHILD | WS_VISIBLE,
            20, 170, 280, 24, window, nullptr, nullptr, nullptr);

        for (HWND control : {g_savedLabel, g_progressBar, g_amountEdit,
                             g_statusLabel}) {
            if (control) {
                SendMessageW(control, WM_SETFONT,
                             reinterpret_cast<WPARAM>(font), TRUE);
            }
        }

        UpdateWidget();
        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == kAddButtonId && HIWORD(wParam) == BN_CLICKED) {
            AddDeposit(window);
            return 0;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(window, message, wParam, lParam);
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_PROGRESS_CLASS};
    InitCommonControlsEx(&controls);

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.hInstance = instance;
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

    HWND window = CreateWindowExW(
        extendedStyle, kWindowClass, L"Money Saver",
        style, CW_USEDEFAULT, CW_USEDEFAULT,
        bounds.right - bounds.left, bounds.bottom - bounds.top,
        nullptr, nullptr, instance, nullptr);

    if (!window) {
        return 1;
    }

    ShowWindow(window, showCommand);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}