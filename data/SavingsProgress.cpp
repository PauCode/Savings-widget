#include "SavingsProgress.h"

#include <algorithm>
#include <cmath>
#include <commctrl.h>
#include <string>

bool SavingsProgress::Create(HWND parent, HFONT font) {
    progressBar_ = CreateWindowExW(
        0, PROGRESS_CLASSW, nullptr, WS_CHILD | WS_VISIBLE,
        20, 76, 230, 18, parent, nullptr, nullptr, nullptr);
    percentageLabel_ = CreateWindowW(
        L"STATIC", L"0%", WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_CENTERIMAGE,
        258, 76, 42, 18, parent, nullptr, nullptr, nullptr);
    if (!progressBar_ || !percentageLabel_) {
        return false;
    }

    SendMessageW(progressBar_, PBM_SETRANGE32, 0, 100);
    SendMessageW(progressBar_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    SendMessageW(percentageLabel_, WM_SETFONT,
                 reinterpret_cast<WPARAM>(font), TRUE);
    return true;
}

void SavingsProgress::Update(int savedCents, int goalCents) const {
    if (!progressBar_ || !percentageLabel_ || goalCents <= 0) {
        return;
    }

    const int progress = std::clamp(
        static_cast<int>(std::lround(
            (static_cast<double>(savedCents) / goalCents) * 100)),
        0, 100);
    SendMessageW(progressBar_, PBM_SETPOS, progress, 0);
    SetWindowTextW(percentageLabel_, (std::to_wstring(progress) + L"%").c_str());
}