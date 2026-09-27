#define UNICODE
#define _UNICODE

#include <windows.h>

#include "MoneySaverWindow.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    MoneySaverWindow application;
    return application.Run(instance, showCommand);
}