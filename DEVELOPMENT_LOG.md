# Development Log

## 2026-09-27

- Separated savings balance and persistence logic into `bin/SavingsData.h` and `bin/SavingsData.cpp`.
- Changed balance storage to four-byte binary cents in `current/savings.dat`; the loader can migrate the legacy `Data/savings.txt` format.
- Organized window and control source files under `data/` and backend services under `bin/`.
- Added `bin/CurrencyRates` for daily USD-based rates for USD, RON, EUR, CAD, RUB, and DKK, with a fallback API endpoint.
- Added five quick-deposit buttons for 50, 100, 250, 500, and 1,000 RON. Manual deposits also use RON; amounts are converted to USD before being saved.
- The UI fetches rates in the background and displays the saved balance and goal in RON after rates load. No currency selector or rate panel is included.
- Updated the build task to create numbered `prototypes/iteration-N` folders containing the executable, compiler/linker artifacts, and `build.log`. The `prototypes/` folder is ignored by Git.
- Validation: MSVC build succeeded through iteration 6; a live API smoke test converted 50 USD to 231.44 RON using rates dated 2026-09-26.
- Note: the binary balance file is not encrypted. Exchange rates update daily, not in real time.
- Added the WebView2 dashboard under `data/ui/` and a selectable default stylesheet under `Theme/`. CSS files with simple alphanumeric, dash, or underscore names are discovered at startup; the selected name is saved in `current/theme.txt`.
- The app now launches WebView2 first and falls back to `NativeFallbackWindow` if the runtime, page navigation, or browser process fails. Savings continue to use `current/savings.dat`; WebView2 profile data is kept under `current/webview2-profile/`.
- The build script restores WebView2 SDK `1.0.4191.47` to the ignored `.packages/` cache and still places each executable and build log in a numbered `prototypes/iteration-N/` folder.
- Validation: iteration 13 built successfully; the WebView2 window launched responsively with the existing savings file present, and editor diagnostics were clear.