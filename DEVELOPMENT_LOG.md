# Development Log

## 2026-09-27

- Separated savings balance and persistence logic into `bin/SavingsData.h` and `bin/SavingsData.cpp`.
- Changed balance storage to four-byte binary cents in `current/savings.dat`; the loader can migrate the legacy `Data/savings.txt` format.
- Organized window and control source files under `data/` and backend services under `bin/`.
- Added `bin/CurrencyRates` for daily USD-based rates for USD, RON, EUR, CAD, RUB, and DKK, with a fallback API endpoint.
- Added six quick-deposit buttons for 50, 100, 250, 500, 1,000, and 1,500 RON. Manual deposits also use RON; amounts are converted to USD before being saved.
- The UI fetches rates in the background and displays the saved balance and goal in RON after rates load. No currency selector or rate panel is included.
- Updated the build task to create numbered `prototypes/iteration-N` folders containing the executable, compiler/linker artifacts, and `build.log`. The `prototypes/` folder is ignored by Git.
- Validation: MSVC build succeeded through iteration 6; a live API smoke test converted 50 USD to 231.44 RON using rates dated 2026-09-26.
- Note: the binary balance file is not encrypted. Exchange rates update daily, not in real time.
- Added the WebView2 dashboard under `data/ui/` and a selectable default stylesheet under `Theme/`. CSS files with simple alphanumeric, dash, or underscore names are discovered at startup; the selected name is saved in `current/theme.txt`.
- The app now launches WebView2 first and falls back to `NativeFallbackWindow` if the runtime, page navigation, or browser process fails. Savings continue to use `current/savings.dat`; WebView2 profile data is kept under `current/webview2-profile/`.
- The build script restores WebView2 SDK `1.0.4191.47` to the ignored `.packages/` cache and still places each executable and build log in a numbered `prototypes/iteration-N/` folder.
- Validation: iteration 13 built successfully; the WebView2 window launched responsively with the existing savings file present, and editor diagnostics were clear.
- Added an Add/Remove mode to both the WebView2 UI and native fallback; preset and custom RON amounts use the selected mode and are converted to USD before storage.
- Added confirmed Reset actions in both UIs. Withdrawals reject amounts above the saved balance, and reset/withdraw operations update the binary balance only after a successful save.
- Validation: iteration 19 built successfully and all edited UI, backend, HTML, JavaScript, and CSS files reported no diagnostics.
- Made all six preset buttons equal-sized; the native fallback lays them out in two rows. Validation: iteration 20 built successfully, diagnostics were clear, and the WebView window launched responsively.
- Added the rounded progress percentage directly on the WebView bar and beside the native fallback bar. Validation: iteration 21 built successfully, UI diagnostics were clear, and the updated app relaunched.
- Added a Settings popover beside Theme with Static, Physics liquid, and Coins jar modes; visual settings persist in the WebView profile. Particle density can be adjusted from 100 to 1,400.
- Added shaded particle-liquid and stacked-coin modes. Custom themes can adjust particle and coin shading through CSS variables.
- Validation: iteration 23 built successfully and the Settings-enabled Savings Jar window launched responsively.
- Replaced the experimental wave-backed liquid with progress-packed, shaded particles. After a swipe, particles move freely under gravity and collision impulses; after 18 quiet frames below the speed threshold, a gentle restoring force returns them toward their fill positions. Particle size also scales for very low balances.
- Validation: iteration 26 built and launched responsively in WebView2 with clear editor diagnostics. Node.js is unavailable, so a separate `node --check` could not be run.
- Increased fluid particle separation to prevent visual overlap, reused spatial collision buckets, and cached shaded particle sprites to reduce per-frame allocation and rendering cost. Validation: iteration 27 built successfully and launched responsively.
- Added uncapped balance-to-goal progress reporting for Physics liquid. The jar stays at its configured particle cap; crossing or exceeding the goal launches a limited set of top particles through the hidden lid, fades them out, and gradually refills their positions.
- Validation: iteration 29 built successfully, editor diagnostics were clear, and the WebView window launched responsively.
- Moved the simple spring-progress particles back into `data/ui/jar-view.js`; JavaScript estimates display refresh from `requestAnimationFrame` and caps drawing at 30 FPS. Swipes are local to the WebView, so particle frames no longer cross the C++ message bridge.
- Removed the unused native spring simulator and per-frame WebView timer. Validation: iteration 39 built successfully, diagnostics were clear, and the WebView window launched responsively. A sustained click could not be simulated with the available desktop tools.
- Removed the Physics liquid/Coins/particle-count Settings options and the old particle renderer. The jar now uses one simple progress-driven wave in `data/ui/wave-view.js`, capped at 30 FPS. Validation: iteration 40 built successfully, diagnostics were clear, and the WebView window launched responsively.
- Fixed the wave's full-progress coordinate: 100% now reaches the jar's inner top edge instead of remaining 54 pixels below it. Validation: iteration 41 built successfully and the WebView window launched responsively.
- Added a translucent side-glass reflection layer above the wave so the liquid no longer obscures the jar walls. Validation: iteration 42 built successfully and the WebView window launched responsively.
- Added a configurable RON savings target persisted with both exact RON cents and its USD-equivalent cents. Legacy four-byte goal files still load, but cannot recover the original RON cents; re-saving the target stores exact precision. Restricted quick-deposit label updates to actual preset buttons, preventing the goal dialog's Cancel button from showing `NaN`.
- Validation: iteration 48 built successfully and the goal-editing WebView launched responsively with clear diagnostics.
- Added a WebView goal editor in RON. The selected target is converted to USD cents and persisted in `current/goal.dat`; missing files default to $1,000, and the native fallback loads the same target. Validation: iteration 45 built successfully, diagnostics were clear, and the updated app launched responsively.

## 2026-10-XX

- Added multi-goal ("jars") support. `bin/GoalManager.h/.cpp` now owns the list of savings goals, the active goal, and per-goal history, backed by `current/goals/<id>/{savings.dat,goal.dat,history.log}` and `current/goals/index.txt`. On first run it migrates the previous single-goal `current/savings.dat` + `current/goal.dat` (and legacy `Data/savings.txt`) into a `default` goal.
- Simplified `bin/SavingsData` to a plain balance store parametrized by directory (`Load(directory)`); it no longer knows about goals, legacy migration, or currencies, since `GoalManager` now owns that logic.
- Savings targets are stored purely in RON cents per goal (no more USD/RON dual-storage), avoiding the earlier currency round-trip precision issue. A brand-new goal's target is finalized to a RON-equivalent of $1,000 the first time exchange rates load.
- The WebView UI (`data/ui/index.html`/`app.js`) gained a goal-pill selector row with a "+ New jar" action (opens a name/target dialog posting a `createGoal` message) and a transaction history panel listing recent deposits/withdrawals/resets per goal, driven by new `selectGoal`/`createGoal` WebView messages and a `goals`/`activeGoalId`/`activeGoalName`/`history` payload in `SendState()`. The jar/wave visualization (`wave-view.js`) was not touched.
- `data/MoneySaverWindow` (native fallback) was updated to use `GoalManager` as well, but keeps operating on whichever goal is currently active in the shared data files (no goal-selector UI in the native fallback).
- Updated `scripts/build.ps1` to compile the new `bin/GoalManager.cpp`.
- Validation: iteration 49 built successfully; the WebView window launched responsively and the migration correctly created `current/goals/default/` with the pre-existing balance/goal, generating a valid `current/goals/index.txt`.
- Fixed a rounding bug where depositing e.g. 100 RON showed as 99.98 RON: balances were stored in USD cents and converted back to RON for display, and quantizing to USD cents on every deposit lost precision on the round trip. Balances are now stored directly in RON cents (like the goal target), removing the round trip entirely. Added a one-time per-goal migration (`balance-unit.txt` marker) that converts any pre-existing USD-cent balance to RON once rates are available, so older goal directories aren't corrupted by the unit change.
- Validation: iteration 50 built successfully and the WebView window launched responsively.
- Added a display-currency selector (WebView only) in place of the green "RON" tag beside the balance, offering all six `CurrencyRates`-supported codes (USD, RON, EUR, CAD, RUB, DKK). Selection persists to `current/currency.txt` and defaults to USD when no file is present. Balance, goal target, preset/custom deposit amounts, and history entries are now converted from RON to/from the selected currency in `WebViewWindow`; internal storage (RON cents) and `GoalManager`/`SavingsData`/`MoneySaverWindow` are untouched, so this is isolated to `data/WebViewWindow.h/.cpp` and the `data/ui/` + `Theme/default.css` frontend for easy revert.
- Validation: iteration 52 built successfully and the WebView window launched responsively; `current/currency.txt` is only written once a currency is explicitly chosen, confirming the USD default.
- Restyled the currency dropdown as a bordered pill (it previously had no border/background and looked identical to plain text, so it wasn't discoverable as a control).
- Added a manual "refresh rates" icon button in the topbar (spinning refresh glyph, tooltip "Refreshes the currency exchange rate API"). It posts a `refreshRates` WebView message that re-runs the existing background rate fetch without blanking the currently displayed balance/goal; a new `ratesRefreshing` state flag disables/spins the button until the fetch completes.
- Validation: iteration 55 built successfully and the WebView window launched responsively.
- Enlarged the jar (`.jar-panel`/`.jar-scene` clamp ranges) and made the whole WebView UI scale with window size: added a fluid `clamp()` root font-size and converted most font-size/padding/gap/margin declarations across the layout to `rem` so they grow together, widened the `.app-shell` cap from 1120px to 1320px, and increased the default native window size to 1280x840 to match. `wave-view.js` already resizes the liquid canvas via `ResizeObserver`, so the jar's wave rendering adapts automatically to the new size without changes.
- Validation: iteration 56 built successfully and the WebView window launched responsively at the larger default size.
- Enlarged the jar further (`.jar-scene` clamp width/height raised substantially, `.savings-view` grid gives the jar column a bigger share) since the account panel's taller content left unused space around it. Sped up `wave-view.js`: wave phase speed roughly tripled and amplitude roughly doubled so the surface is visibly animated instead of near-static, and the fill-level spring (progress acceleration/damping) was tightened so the liquid catches up to deposits/withdrawals noticeably faster.
- Validation: iteration 57 built successfully and the WebView window launched responsively.





