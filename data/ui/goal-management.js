(() => {
    const goalSelect = document.querySelector("#goal-select");
    const deleteGoalButton = document.querySelector("#delete-goal-button");
    const renameGoalButton = document.querySelector("#rename-goal-button");
    const renameGoalDialog = document.querySelector("#rename-goal-dialog");
    const renameGoalForm = document.querySelector("#rename-goal-form");
    const renameGoalName = document.querySelector("#rename-goal-name");
    const renameGoalError = document.querySelector("#rename-goal-error");
    const cancelRenameGoal = document.querySelector("#cancel-rename-goal");
    const completedGoalButton = document.querySelector("#completed-goal-button");
    const completedGoalDialog = document.querySelector("#completed-goal-dialog");
    const archiveCompletedButton = document.querySelector("#archive-completed-goal");
    const deleteCompletedButton = document.querySelector("#delete-completed-goal");
    const keepCompletedButton = document.querySelector("#keep-completed-goal");
    const createFromCompletedButton = document.querySelector("#create-from-completed-goal");
    const completedGoalMessage = document.querySelector("#completed-goal-message");
    const createGoalButton = document.querySelector("#create-goal-button");
    const showArchivedButton = document.querySelector("#show-archived-button");
    const archivedDialog = document.querySelector("#archived-goals-dialog");
    const archivedList = document.querySelector("#archived-goals-list");
    const archivedEmpty = document.querySelector("#archived-goals-empty");
    const closeArchivedButton = document.querySelector("#close-archived-button");

    let activeGoalId = "";
    let activeGoalName = "";
    let hasAlternativeGoal = false;
    let completionWarningActive = false;
    let completionWarningTimer = 0;

    const completionPrompt = "Do you want to archive, delete or keep this jar?";
    const noOtherJarsMessage = "Cannot delete/archive jar as there aren't any other jars remaining. Please create another jar and try again.";

    function postMessage(message) {
        window.chrome?.webview?.postMessage(message);
    }

    goalSelect.addEventListener("change", () => {
        if (goalSelect.value && goalSelect.value !== activeGoalId) {
            postMessage({ type: "selectGoal", id: goalSelect.value });
        }
    });

    deleteGoalButton.addEventListener("click", () => {
        if (activeGoalId && window.confirm("Delete the selected jar permanently?")) {
            postMessage({ type: "deleteGoal", id: activeGoalId });
        }
    });

    renameGoalButton.addEventListener("click", () => {
        renameGoalError.textContent = "";
        renameGoalName.value = activeGoalName;
        renameGoalDialog.showModal();
        renameGoalName.select();
    });

    cancelRenameGoal.addEventListener("click", () => renameGoalDialog.close());

    renameGoalForm.addEventListener("submit", (event) => {
        event.preventDefault();
        const name = renameGoalName.value.trim();
        if (!name) {
            renameGoalError.textContent = "Enter a jar name.";
            return;
        }
        postMessage({ type: "renameGoal", id: activeGoalId, name });
        renameGoalDialog.close();
    });

    completedGoalButton.addEventListener("click", () => {
        resetCompletionWarning();
        completedGoalDialog.showModal();
    });

    archiveCompletedButton.addEventListener("click", () => {
        if (!hasAlternativeGoal) {
            showCompletionWarning();
            return;
        }
        if (activeGoalId) {
            postMessage({ type: "archiveGoal", id: activeGoalId });
            completedGoalDialog.close();
        }
    });

    deleteCompletedButton.addEventListener("click", () => {
        if (!hasAlternativeGoal) {
            showCompletionWarning();
            return;
        }
        if (activeGoalId) {
            postMessage({ type: "deleteGoal", id: activeGoalId });
            completedGoalDialog.close();
        }
    });

    keepCompletedButton.addEventListener("click", () => {
        resetCompletionWarning();
        completedGoalDialog.close();
    });

    completedGoalDialog.addEventListener("close", resetCompletionWarning);

    createFromCompletedButton.addEventListener("click", () => {
        resetCompletionWarning();
        completedGoalDialog.close();
        createGoalButton.click();
    });

    function showCompletionWarning() {
        if (completionWarningActive) {
            return;
        }
        completionWarningActive = true;
        completedGoalMessage.textContent = noOtherJarsMessage;
        completedGoalMessage.classList.add("is-warning");
        createFromCompletedButton.hidden = false;
        completionWarningTimer = window.setTimeout(() => {
            completedGoalMessage.classList.add("is-fading");
            window.setTimeout(resetCompletionWarning, 220);
        }, 5000);
    }

    function resetCompletionWarning() {
        window.clearTimeout(completionWarningTimer);
        completionWarningTimer = 0;
        completionWarningActive = false;
        completedGoalMessage.textContent = completionPrompt;
        completedGoalMessage.classList.remove("is-warning", "is-fading");
        createFromCompletedButton.hidden = true;
    }

    showArchivedButton.addEventListener("click", () => archivedDialog.showModal());
    closeArchivedButton.addEventListener("click", () => archivedDialog.close());

    function renderArchivedGoals(goals) {
        archivedList.replaceChildren();
        archivedEmpty.hidden = goals.length > 0;
        goals.forEach((goal) => {
            const item = document.createElement("li");
            item.textContent = goal.name;
            archivedList.append(item);
        });
    }

    window.goalManagement = {
        render({
            goals,
            activeGoalId: selectedId,
            archivedGoals,
            canArchiveActiveGoal,
            isActiveGoalComplete
        }) {
            activeGoalId = selectedId;
            activeGoalName = goals.find((goal) => goal.id === selectedId)?.name || "";

            goalSelect.replaceChildren();
            goals.forEach((goal) => {
                const option = document.createElement("option");
                option.value = goal.id;
                option.textContent = goal.name;
                goalSelect.append(option);
            });
            goalSelect.value = selectedId;

            hasAlternativeGoal = goals.length > 1;
            deleteGoalButton.disabled = !hasAlternativeGoal;
            completedGoalButton.hidden = !isActiveGoalComplete;
            archiveCompletedButton.classList.toggle("is-unavailable", !canArchiveActiveGoal);
            archiveCompletedButton.setAttribute("aria-disabled", String(!canArchiveActiveGoal));
            deleteCompletedButton.classList.toggle("is-unavailable", !hasAlternativeGoal);
            deleteCompletedButton.setAttribute("aria-disabled", String(!hasAlternativeGoal));
            renderArchivedGoals(archivedGoals);
        }
    };
})();
