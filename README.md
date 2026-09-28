# Savings widget
This widget is created as a way to make it easy for people to save money, also it is for people that spend too much time on their computer and can't be bothered to use a phone.

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
