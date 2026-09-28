# Savings Jar

Savings Jar is a small Windows desktop app that makes saving money simple and visual. It is made for people who spend a lot of time on their computer and would rather track their savings there than on a phone.

Create a jar for each thing you are saving for, set a target, and add or remove money as you go. The jar fills with animated liquid as you get closer to your goal.

[screenshot of the main window with a jar partly filled here]

## Features

- **Multiple jars**: Keep a separate jar for each goal (a guitar, a holiday, an emergency fund) and switch between them from a dropdown. Jars can be renamed, and names are limited to 30 characters.

  [screenshot of the jar dropdown and jar controls here]

- **Targets and progress**: Each jar has a target amount. The app shows your current balance, the remaining amount to reach the goal, and a progress bar with a percentage.

  [screenshot of the balance, remaining amount and progress bar here]

- **Quick deposits and withdrawals**: Use the preset buttons (50 to 1,500) or enter a custom amount. Switch between **Add** and **Remove** to deposit or withdraw.

  [screenshot of the Add/Remove toggle and preset buttons here]

- **Multiple currencies**: View and enter amounts in USD, RON, EUR, CAD, RUB or DKK. Exchange rates are downloaded daily and can be refreshed manually. You can pick the currency when creating a new jar.

  [screenshot of the currency dropdown here]

- **Recent activity**: Each jar keeps a history of deposits, withdrawals and resets. The five most recent entries are visible, and you can scroll to see older ones.

  [screenshot of the recent activity panel here]

- **Completing a jar**: When a jar reaches its target, an **Archive or delete** button appears. You can archive the jar to keep a record of it, delete it, or keep using it. Archived jars can be viewed from **Archived jars**.

  [screenshot of the completed jar dialog here]

- **Monthly reminder**: Set up a Windows notification on a chosen day each month to remind you to save. Keep the default message or write your own.

  [screenshot of the Create notification dialog here]

  [screenshot of the Windows notification here]

- **Runs in the background**: When you press X, you can choose to close the app or hide it in the system tray. Open it again from the tray icon or by launching the app again; only one copy runs at a time.

  [screenshot of the tray icon in the hidden icons area here]

- **Start with Windows**: On first launch, the app asks whether it should start automatically when you sign in.

- **Themes**: Choose a theme from the theme menu, or add your own (see [Themes](#themes)).

## Getting started

1. Download `SavingsJar-Windows-x64.zip` and extract it anywhere.
2. Run `Savings Jar.exe`.
3. Choose whether the app should start with Windows.
4. Create your first jar by giving it a name, a target amount and a currency.

[screenshot of the first-run Create jar dialog here]

**Requirements:** Windows 10 or 11 with the [Microsoft Edge WebView2 Runtime](https://developer.microsoft.com/microsoft-edge/webview2/) installed (it is included with Windows 11 and most up-to-date Windows 10 systems).

Your data is saved in `%APPDATA%\PauCode\Savings Jar`. Deleting this folder removes all jars, balances, history and settings.

## Project board

[Trello board](https://trello.com/b/EXYHdcAu/ga)

## Themes

Add a theme folder to [Theme/](Theme/README.md). The app discovers themes at startup and lets you select one from the theme menu.

## Build and fallback

Run the default VS Code build task. Each build creates a portable runtime folder at `prototypes/iteration-N/Savings Jar/` and a ready-to-share `SavingsJar-Windows-x64.zip` beside it. The package contains only the executable, WebView UI, and themes; compiler artifacts remain under `iteration-N/build/`.

Writable data is stored in `%APPDATA%\PauCode\Savings Jar`, including goals, preferences, and the WebView2 profile. A first run starts with no jars and opens the Create jar dialog; repository data under `current/` is never imported. The WebView2 Runtime must be installed to use the web UI; if WebView2 initialization, page loading, or its browser process fails, the native UI starts instead.

Before the first jar is created, the app asks whether it should start with Windows. Choosing Yes adds a per-user `PauCode Savings Jar` entry under `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`; choosing No leaves startup disabled. The answer is stored in `%APPDATA%\PauCode\Savings Jar\start-with-windows.txt` so the question is only asked once.

## Monthly notification

Use **Create notification** beside the jar controls to pick a day of the month and either keep the default reminder text or write a custom message. The reminder is delivered as a standard Windows notification through the app's tray icon, fires once per calendar month, and falls back to the last day in shorter months. Settings are stored in `%APPDATA%\PauCode\Savings Jar\reminder.txt`.

## Development log

See [DEVELOPMENT_LOG.md](DEVELOPMENT_LOG.md) for implementation notes and verification history.
