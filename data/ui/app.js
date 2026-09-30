const themeSelect = document.querySelector("#theme-select");
const themeStylesheet = document.querySelector("#theme-stylesheet");
const depositForm = document.querySelector("#deposit-form");
const depositAmount = document.querySelector("#deposit-amount");
const applyButton = document.querySelector("#apply-button");
const statusMessage = document.querySelector("#status-message");
const presetButtons = document.querySelectorAll(".preset-grid .preset-button");
const addModeButton = document.querySelector("#add-mode");
const removeModeButton = document.querySelector("#remove-mode");
const modeToggle = document.querySelector(".mode-toggle");
const noteToggle = document.querySelector("#note-toggle");
const depositNote = document.querySelector("#deposit-note");
const depositTitle = document.querySelector("#deposit-title");
const resetButton = document.querySelector("#reset-button");
const balanceValue = document.querySelector("#balance-value");
const remainingValue = document.querySelector("#remaining-value");
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
const createGoalButton = document.querySelector("#create-goal-button");
const createGoalDialog = document.querySelector("#create-goal-dialog");
const createGoalForm = document.querySelector("#create-goal-form");
const createGoalName = document.querySelector("#create-goal-name");
const createGoalTarget = document.querySelector("#create-goal-target");
const createGoalCurrency = document.querySelector("#create-goal-currency");
const createGoalError = document.querySelector("#create-goal-error");
const cancelCreateGoalButton = document.querySelector("#cancel-create-goal");
const submitCreateGoalButton = document.querySelector("#submit-create-goal");
const historyList = document.querySelector("#history-list");
const historyEmpty = document.querySelector("#history-empty");
const currencySelect = document.querySelector("#currency-select");
const currencyTag = document.querySelector("#currency-tag");
const goalAmountLabel = document.querySelector("#goal-amount-label");
const createGoalTargetLabel = document.querySelector("#create-goal-target-label");
const refreshRatesButton = document.querySelector("#refresh-rates-button");
let activeMode = "add";
let currentCurrency = "RON";

function postMessage(message) {
    if (window.chrome?.webview) {
        window.chrome.webview.postMessage(message);
    }
}

function formatAmount(amount) {
    return new Intl.NumberFormat("ro-RO", {
        minimumFractionDigits: 2,
        maximumFractionDigits: 2
    }).format(amount);
}

function setDepositEnabled(enabled) {
    depositAmount.disabled = !enabled;
    applyButton.disabled = !enabled;
    depositNote.disabled = !enabled;
    presetButtons.forEach((button) => {
        button.disabled = !enabled;
    });
}

function takeNote() {
    if (!noteToggle.checked) {
        return "";
    }
    const note = depositNote.value.trim();
    depositNote.value = "";
    return note;
}

function updatePresetLabels() {
    const adding = activeMode === "add";
    presetButtons.forEach((button) => {
        const amount = new Intl.NumberFormat("en-US").format(Number(button.dataset.amount));
        button.textContent = `${adding ? "+" : "-"}${amount} ${currentCurrency}`;
    });
}

function setMode(mode) {
    activeMode = mode;
    const adding = activeMode === "add";
    addModeButton.classList.toggle("is-active", adding);
    removeModeButton.classList.toggle("is-active", !adding);
    modeToggle.classList.toggle("is-remove", !adding);
    addModeButton.setAttribute("aria-pressed", String(adding));
    removeModeButton.setAttribute("aria-pressed", String(!adding));
    depositTitle.textContent = adding ? "Add to your jar" : "Remove from your jar";
    applyButton.textContent = adding ? "Add deposit" : "Remove amount";
    updatePresetLabels();
}

function setCurrencyOptions(currencies, selectedCurrency) {
    [currencySelect, createGoalCurrency].forEach((select) => {
        const previous = select.value;
        select.replaceChildren();
        currencies.forEach((code) => {
            const option = document.createElement("option");
            option.value = code;
            option.textContent = code;
            select.append(option);
        });
        select.value = select === createGoalCurrency && createGoalDialog.open && previous
            ? previous
            : selectedCurrency;
    });
}

function setThemeOptions(themes, selectedTheme) {
    themeSelect.replaceChildren();
    themes.forEach((theme) => {
        const option = document.createElement("option");
        option.value = theme;
        option.textContent = theme;
        themeSelect.append(option);
    });
    themeSelect.value = selectedTheme;
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
            : `${sign}${formatAmount(Math.abs(entry.amountRon))} ${currentCurrency}`;

        const labelSpan = document.createElement("span");
        labelSpan.className = "history-label";
        labelSpan.textContent = label;

        const dateSpan = document.createElement("span");
        dateSpan.className = "history-date";
        dateSpan.textContent = formatHistoryDate(entry.timestamp);

        const amountSpan = document.createElement("span");
        amountSpan.className = `history-amount ${entry.type}`;
        amountSpan.textContent = amountText;

        const noteSpan = document.createElement("span");
        noteSpan.className = "history-note";
        noteSpan.textContent = entry.note || "";
        if (entry.note) {
            noteSpan.title = entry.note;
        }

        item.append(labelSpan, dateSpan, noteSpan, amountSpan);
        historyList.append(item);
    });
}

function renderState(state) {
    if (state.themes?.length) {
        setThemeOptions(state.themes, state.selectedTheme);
    }

    if (state.selectedTheme) {
        themeStylesheet.href = `/Theme/${encodeURIComponent(state.selectedTheme)}/${encodeURIComponent(state.selectedTheme)}.css`;
    }

    if (state.goals) {
        window.goalManagement?.render({
            goals: state.goals,
            activeGoalId: state.activeGoalId,
            archivedGoals: state.archivedGoals || [],
            canArchiveActiveGoal: Boolean(state.canArchiveActiveGoal),
            isActiveGoalComplete: Boolean(state.isActiveGoalComplete),
            startupChoiceRequired: Boolean(state.startupChoiceRequired)
        });
    }
    const hasActiveGoal = Boolean(state.activeGoalId);
    goalTitle.textContent = hasActiveGoal
        ? state.activeGoalName
        : "Create your first jar";

    if (state.currencies?.length) {
        setCurrencyOptions(state.currencies, state.currency);
    }
    if (state.currency) {
        currentCurrency = state.currency;
        currencyTag.textContent = currentCurrency;
        goalAmountLabel.textContent = `New goal amount (${currentCurrency})`;
        if (!createGoalDialog.open) {
            createGoalTargetLabel.textContent = `Target amount (${currentCurrency})`;
        }
        updatePresetLabels();
    }

    if (state.history) {
        renderHistory(state.history);
    }

    refreshRatesButton.disabled = Boolean(state.ratesRefreshing);
    refreshRatesButton.classList.toggle("is-spinning", Boolean(state.ratesRefreshing));

    setDepositEnabled(state.ratesAvailable && hasActiveGoal);
    editGoalButton.disabled = !hasActiveGoal;
    resetButton.disabled = !hasActiveGoal;
    addModeButton.disabled = !hasActiveGoal;
    removeModeButton.disabled = !hasActiveGoal;
    submitCreateGoalButton.disabled = !state.ratesAvailable;
    rateDate.textContent = state.ratesAvailable
        ? `Rates · ${state.rateDate}`
        : state.ratesLoading ? "Rates loading" : "Rates unavailable";
    const percent = Math.min(100, Math.max(0, state.progressPercent));
    progressValue.style.width = `${percent}%`;
    progressLabel.textContent = `${Math.round(percent)}%`;
    const progressRatio = state.progressRatio ?? percent;
    waveView.setProgress(progressRatio);

    if (!hasActiveGoal) {
        balanceValue.textContent = "--.--";
        remainingValue.textContent = "--.--";
        goalValue.textContent = "--.--";
        statusMessage.textContent = "Create a jar to start saving.";
        if (createGoalDialog.open) {
            createGoalError.textContent = state.ratesAvailable
                ? ""
                : "Waiting for exchange rates...";
        }
        return;
    }

    if (!state.ratesAvailable) {
        balanceValue.textContent = "--.--";
        remainingValue.textContent = "--.--";
        goalValue.textContent = "--.--";
        statusMessage.textContent = state.ratesLoading
            ? "Connecting to exchange rates..."
            : "Exchange rates are unavailable. Deposits are disabled.";
        return;
    }

    balanceValue.textContent = formatAmount(state.balanceRon);
    remainingValue.textContent = `${formatAmount(state.remainingRon)} ${currentCurrency}`;
    goalValue.textContent = `${formatAmount(state.goalRon)} ${currentCurrency}`;
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

currencySelect.addEventListener("change", () => {
    postMessage({ type: "currency", code: currencySelect.value });
});

refreshRatesButton.addEventListener("click", () => {
    postMessage({ type: "refreshRates" });
});

addModeButton.addEventListener("click", () => setMode("add"));
removeModeButton.addEventListener("click", () => setMode("remove"));

presetButtons.forEach((button) => {
    button.addEventListener("click", () => {
        postMessage({
            type: activeMode === "add" ? "deposit" : "withdraw",
            amount: Number(button.dataset.amount),
            note: takeNote()
        });
    });
});

depositForm.addEventListener("submit", (event) => {
    event.preventDefault();
    postMessage({
        type: activeMode === "add" ? "deposit" : "withdraw",
        amount: Number(depositAmount.value),
        note: takeNote()
    });
});

noteToggle.addEventListener("change", () => {
    depositNote.hidden = !noteToggle.checked;
    if (noteToggle.checked) {
        depositNote.focus();
    } else {
        depositNote.value = "";
    }
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
    const amount = Number(goalAmount.value);
    if (!Number.isFinite(amount) || amount <= 0 || amount > 1000000) {
        goalError.textContent = `Enter a goal between 0.01 and 1,000,000 ${currentCurrency}.`;
        return;
    }
    postMessage({ type: "goal", amount });
    goalDialog.close();
});

createGoalButton.addEventListener("click", () => {
    createGoalError.textContent = "";
    createGoalForm.reset();
    createGoalCurrency.value = currentCurrency;
    createGoalTargetLabel.textContent = `Target amount (${currentCurrency})`;
    createGoalDialog.showModal();
    createGoalName.focus();
});

cancelCreateGoalButton.addEventListener("click", () => {
    createGoalDialog.close();
});

createGoalCurrency.addEventListener("change", () => {
    createGoalTargetLabel.textContent = `Target amount (${createGoalCurrency.value})`;
});

createGoalForm.addEventListener("submit", (event) => {
    event.preventDefault();
    const name = createGoalName.value.trim();
    const target = Number(createGoalTarget.value);
    if (!name) {
        createGoalError.textContent = "Enter a name for the jar.";
        return;
    }
    if (!Number.isFinite(target) || target <= 0 || target > 1000000) {
        createGoalError.textContent = `Enter a target between 0.01 and 1,000,000 ${createGoalCurrency.value}.`;
        return;
    }
    // The backend converts the target from the display currency, so switch it first.
    if (createGoalCurrency.value && createGoalCurrency.value !== currentCurrency) {
        postMessage({ type: "currency", code: createGoalCurrency.value });
    }
    postMessage({ type: "createGoal", name, target });
    createGoalDialog.close();
});

setMode("add");
postMessage({ type: "ready" });