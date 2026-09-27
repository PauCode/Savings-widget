const themeSelect = document.querySelector("#theme-select");
const themeStylesheet = document.querySelector("#theme-stylesheet");
const depositForm = document.querySelector("#deposit-form");
const depositAmount = document.querySelector("#deposit-amount");
const applyButton = document.querySelector("#apply-button");
const statusMessage = document.querySelector("#status-message");
const presetButtons = document.querySelectorAll(".preset-button");
const addModeButton = document.querySelector("#add-mode");
const removeModeButton = document.querySelector("#remove-mode");
const depositTitle = document.querySelector("#deposit-title");
const resetButton = document.querySelector("#reset-button");
const balanceValue = document.querySelector("#balance-value");
const goalValue = document.querySelector("#goal-value");
const progressValue = document.querySelector("#progress-value");
const progressLabel = document.querySelector("#progress-label");
const progressTrack = document.querySelector(".progress-track");
const waveView = new WaveView(document.querySelector("#jar-wave"));
const rateDate = document.querySelector("#rate-date");
let activeMode = "add";

function postMessage(message) {
    if (window.chrome?.webview) {
        window.chrome.webview.postMessage(message);
    }
}

function formatRon(amount) {
    return new Intl.NumberFormat("ro-RO", {
        minimumFractionDigits: 2,
        maximumFractionDigits: 2
    }).format(amount);
}

function setDepositEnabled(enabled) {
    depositAmount.disabled = !enabled;
    applyButton.disabled = !enabled;
    presetButtons.forEach((button) => {
        button.disabled = !enabled;
    });
}

function setMode(mode) {
    activeMode = mode;
    const adding = activeMode === "add";
    addModeButton.classList.toggle("is-active", adding);
    removeModeButton.classList.toggle("is-active", !adding);
    addModeButton.setAttribute("aria-pressed", String(adding));
    removeModeButton.setAttribute("aria-pressed", String(!adding));
    depositTitle.textContent = adding ? "Add to your jar" : "Remove from your jar";
    applyButton.textContent = adding ? "Add deposit" : "Remove amount";

    presetButtons.forEach((button) => {
        const amount = new Intl.NumberFormat("en-US").format(Number(button.dataset.amount));
        button.textContent = `${adding ? "+" : "-"}${amount} RON`;
    });
}

function setThemeOptions(themes, selectedTheme) {
    themeSelect.replaceChildren();
    themes.forEach((theme) => {
        const option = document.createElement("option");
        option.value = theme;
        option.textContent = theme.replace(/\.css$/i, "");
        themeSelect.append(option);
    });
    themeSelect.value = selectedTheme;
}

function renderState(state) {
    if (state.themes?.length) {
        setThemeOptions(state.themes, state.selectedTheme);
    }

    if (state.selectedTheme) {
        themeStylesheet.href = `/Theme/${encodeURIComponent(state.selectedTheme)}`;
    }

    setDepositEnabled(state.ratesAvailable);
    rateDate.textContent = state.ratesAvailable
        ? `Rates · ${state.rateDate}`
        : state.ratesLoading ? "Rates loading" : "Rates unavailable";
    const percent = Math.min(100, Math.max(0, state.progressPercent));
    progressValue.style.width = `${percent}%`;
    progressLabel.textContent = `${Math.round(percent)}%`;
    const progressRatio = state.progressRatio ?? percent;
    waveView.setProgress(progressRatio);

    if (!state.ratesAvailable) {
        balanceValue.textContent = "--.--";
        goalValue.textContent = "--.-- RON";
        statusMessage.textContent = state.ratesLoading
            ? "Connecting to exchange rates..."
            : "RON rates are unavailable. Deposits are disabled.";
        return;
    }

    balanceValue.textContent = formatRon(state.balanceRon);
    goalValue.textContent = `${formatRon(state.goalRon)} RON`;

    progressTrack.setAttribute("aria-valuenow", String(Math.round(percent)));
    statusMessage.textContent = state.status || "Every deposit gets you closer.";
}

window.chrome?.webview?.addEventListener("message", (event) => {
    if (event.data?.type === "state") {
        renderState(event.data);
    }
});

themeSelect.addEventListener("change", () => {
    postMessage({ type: "theme", file: themeSelect.value });
});

addModeButton.addEventListener("click", () => setMode("add"));
removeModeButton.addEventListener("click", () => setMode("remove"));

presetButtons.forEach((button) => {
    button.addEventListener("click", () => {
        postMessage({
            type: activeMode === "add" ? "deposit" : "withdraw",
            amountRon: Number(button.dataset.amount)
        });
    });
});

depositForm.addEventListener("submit", (event) => {
    event.preventDefault();
    postMessage({
        type: activeMode === "add" ? "deposit" : "withdraw",
        amountRon: Number(depositAmount.value)
    });
});

resetButton.addEventListener("click", () => {
    postMessage({ type: "reset" });
});

setMode("add");
postMessage({ type: "ready" });