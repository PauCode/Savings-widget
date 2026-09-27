#include "SavingsProgress.h"

#include <algorithm>
#include <commctrl.h>

bool SavingsProgress::Create(HWND parent, HFONT font) {
    progressBar_ = CreateWindowExW(
        0, PROGRESS_CLASSW, nullptr, WS_CHILD | WS_VISIBLE,
        20, 76, 280, 18, parent, nullptr, nullptr, nullptr);
    if (!progressBar_) {
        return false;
    }

    SendMessageW(progressBar_, PBM_SETRANGE32, 0, 100);
    SendMessageW(progressBar_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    return true;
}

void SavingsProgress::Update(int savedCents, int goalCents) const {
    if (!progressBar_ || goalCents <= 0) {
        return;
    }

    const int progress = std::clamp(
        static_cast<int>((static_cast<double>(savedCents) / goalCents) * 100),
        0, 100);
    SendMessageW(progressBar_, PBM_SETPOS, progress, 0);
}