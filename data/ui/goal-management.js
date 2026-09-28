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
    const showArchivedButton = document.querySelector("#show-archived-button");
    const archivedDialog = document.querySelector("#archived-goals-dialog");
    const archivedList = document.querySelector("#archived-goals-list");
    const archivedEmpty = document.querySelector("#archived-goals-empty");
    const closeArchivedButton = document.querySelector("#close-archived-button");

    let activeGoalId = "";
    let activeGoalName = "";

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
        completedGoalDialog.showModal();
    });

    archiveCompletedButton.addEventListener("click", () => {
        if (activeGoalId) {
            postMessage({ type: "archiveGoal", id: activeGoalId });
            completedGoalDialog.close();
        }
    });

    deleteCompletedButton.addEventListener("click", () => {
        if (activeGoalId) {
            postMessage({ type: "deleteGoal", id: activeGoalId });
            completedGoalDialog.close();
        }
    });

    keepCompletedButton.addEventListener("click", () => completedGoalDialog.close());

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

            const hasAlternative = goals.length > 1;
            deleteGoalButton.disabled = !hasAlternative;
            completedGoalButton.hidden = !isActiveGoalComplete;
            archiveCompletedButton.disabled = !canArchiveActiveGoal;
            deleteCompletedButton.disabled = !hasAlternative;
            renderArchivedGoals(archivedGoals);
        }
    };
})();
