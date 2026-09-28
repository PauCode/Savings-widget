#define UNICODE
#define _UNICODE

#include <windows.h>

#include "../bin/SingleInstance.h"
#include "MoneySaverWindow.h"
#include "WebViewWindow.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    SingleInstanceGuard singleInstance;
    if (!singleInstance.Acquire()) {
        return 0;
    }

    {
        WebViewWindow webView;
        int exitCode = 0;
        if (webView.Run(instance, showCommand, exitCode)) {
            return exitCode;
        }
    }

    NativeFallbackWindow application;
    return application.Run(instance, showCommand);
}