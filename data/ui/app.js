const themeSelect = document.querySelector("#theme-select");
const themeStylesheet = document.querySelector("#theme-stylesheet");
const depositForm = document.querySelector("#deposit-form");
const depositAmount = document.querySelector("#deposit-amount");
const statusMessage = document.querySelector("#status-message");
const presetButtons = document.querySelectorAll(".preset-button");
const balanceValue = document.querySelector("#balance-value");
const goalValue = document.querySelector("#goal-value");
const progressValue = document.querySelector("#progress-value");
const progressPercent = document.querySelector("#progress-percent");
const progressTrack = document.querySelector(".progress-track");
const jarFill = document.querySelector("#jar-fill");
const rateDate = document.querySelector("#rate-date");

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
    depositForm.querySelector("button").disabled = !enabled;
    presetButtons.forEach((button) => {
        button.disabled = !enabled;
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

    const percent = Math.min(100, Math.max(0, state.progressPercent));
    progressValue.style.width = `${percent}%`;
    jarFill.style.height = `${percent}%`;
    progressPercent.textContent = `${Math.round(percent)}%`;
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

presetButtons.forEach((button) => {
    button.addEventListener("click", () => {
        postMessage({ type: "deposit", amountRon: Number(button.dataset.amount) });
    });
});

depositForm.addEventListener("submit", (event) => {
    event.preventDefault();
    postMessage({ type: "deposit", amountRon: Number(depositAmount.value) });
});

postMessage({ type: "ready" });