const themeSelect = document.querySelector("#theme-select");
const themeStylesheet = document.querySelector("#theme-stylesheet");
const depositForm = document.querySelector("#deposit-form");
const depositAmount = document.querySelector("#deposit-amount");
const applyButton = document.querySelector("#apply-button");
const statusMessage = document.querySelector("#status-message");
const presetButtons = document.querySelectorAll(".preset-grid .preset-button");
const addModeButton = document.querySelector("#add-mode");
const removeModeButton = document.querySelector("#remove-mode");
const depositTitle = document.querySelector("#deposit-title");
const resetButton = document.querySelector("#reset-button");
const balanceValue = document.querySelector("#balance-value");
const goalValue = document.querySelector("#goal-value");
const editGoalButton = document.querySelector("#edit-goal-button");
const goalDialog = document.querySelector("#goal-dialog");
const goalForm = document.querySelector("#goal-form");
const goalAmount = document.querySelector("#goal-amount");
const goalError = document.querySelector("#goal-error");
const cancelGoalButton = document.querySelector("#cancel-goal");
const progressValue = document.querySelector("#progress-value");
const progressLabel = document.querySelector("#progress-label");
const progressTrack = document.querySelector(".progress-track");
const waveView = new WaveView(document.querySelector("#jar-wave"));
const rateDate = document.querySelector("#rate-date");
const goalTitle = document.querySelector("#goal-title");
const goalPillsContainer = document.querySelector("#goal-pills");
const createGoalButton = document.querySelector("#create-goal-button");
const createGoalDialog = document.querySelector("#create-goal-dialog");
const createGoalForm = document.querySelector("#create-goal-form");
const createGoalName = document.querySelector("#create-goal-name");
const createGoalTarget = document.querySelector("#create-goal-target");
const createGoalError = document.querySelector("#create-goal-error");
const cancelCreateGoalButton = document.querySelector("#cancel-create-goal");
const historyList = document.querySelector("#history-list");
const historyEmpty = document.querySelector("#history-empty");
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

function renderGoalPills(goals, activeGoalId) {
    goalPillsContainer.replaceChildren();
    goals.forEach((goal) => {
        const pill = document.createElement("button");
        pill.type = "button";
        pill.className = "goal-pill";
        pill.classList.toggle("is-active", goal.id === activeGoalId);
        pill.setAttribute("aria-pressed", String(goal.id === activeGoalId));
        pill.textContent = goal.name;
        pill.addEventListener("click", () => {
            if (goal.id !== activeGoalId) {
                postMessage({ type: "selectGoal", id: goal.id });
            }
        });
        goalPillsContainer.append(pill);
    });
}

function formatHistoryDate(timestampMillis) {
    const date = new Date(timestampMillis);
    const datePart = date.toLocaleDateString(undefined, { day: "2-digit", month: "2-digit" });
    const timePart = date.toLocaleTimeString(undefined, { hour: "2-digit", minute: "2-digit" });
    return `${datePart}, ${timePart}`;
}

function renderHistory(history) {
    historyList.replaceChildren();
    historyEmpty.hidden = history.length > 0;
    history.forEach((entry) => {
        const item = document.createElement("li");
        item.className = "history-item";

        const label = entry.type === "deposit"
            ? "Deposit"
            : entry.type === "withdraw" ? "Withdrawal" : "Reset";
        const sign = entry.type === "withdraw" ? "-" : entry.type === "deposit" ? "+" : "";
        const amountText = entry.type === "reset"
            ? "Balance set to 0"
            : `${sign}${formatRon(Math.abs(entry.amountRon))} RON`;

        const labelSpan = document.createElement("span");
        labelSpan.className = "history-label";
        labelSpan.textContent = label;

        const dateSpan = document.createElement("span");
        dateSpan.className = "history-date";
        dateSpan.textContent = formatHistoryDate(entry.timestamp);

        const amountSpan = document.createElement("span");
        amountSpan.className = `history-amount ${entry.type}`;
        amountSpan.textContent = amountText;

        item.append(labelSpan, dateSpan, amountSpan);
        historyList.append(item);
    });
}

function renderState(state) {
    if (state.themes?.length) {
        setThemeOptions(state.themes, state.selectedTheme);
    }

    if (state.selectedTheme) {
        themeStylesheet.href = `/Theme/${encodeURIComponent(state.selectedTheme)}`;
    }

    if (state.goals) {
        renderGoalPills(state.goals, state.activeGoalId);
    }
    if (state.activeGoalName) {
        goalTitle.textContent = state.activeGoalName;
    }
    if (state.history) {
        renderHistory(state.history);
    }

    setDepositEnabled(state.ratesAvailable);
    editGoalButton.disabled = false;
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
    if (!goalDialog.open) {
        goalAmount.value = Number(state.goalRon).toFixed(2);
    }

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

editGoalButton.addEventListener("click", () => {
    goalError.textContent = "";
    goalDialog.showModal();
    goalAmount.focus();
});

cancelGoalButton.addEventListener("click", () => {
    goalDialog.close();
});

goalForm.addEventListener("submit", (event) => {
    event.preventDefault();
    const amountRon = Number(goalAmount.value);
    if (!Number.isFinite(amountRon) || amountRon <= 0 || amountRon > 1000000) {
        goalError.textContent = "Enter a goal between 0.01 and 1,000,000 RON.";
        return;
    }
    postMessage({ type: "goal", amountRon });
    goalDialog.close();
});

createGoalButton.addEventListener("click", () => {
    createGoalError.textContent = "";
    createGoalForm.reset();
    createGoalDialog.showModal();
    createGoalName.focus();
});

cancelCreateGoalButton.addEventListener("click", () => {
    createGoalDialog.close();
});

createGoalForm.addEventListener("submit", (event) => {
    event.preventDefault();
    const name = createGoalName.value.trim();
    const targetRon = Number(createGoalTarget.value);
    if (!name) {
        createGoalError.textContent = "Enter a name for the jar.";
        return;
    }
    if (!Number.isFinite(targetRon) || targetRon <= 0 || targetRon > 1000000) {
        createGoalError.textContent = "Enter a target between 0.01 and 1,000,000 RON.";
        return;
    }
    postMessage({ type: "createGoal", name, targetRon });
    createGoalDialog.close();
});

setMode("add");
postMessage({ type: "ready" });