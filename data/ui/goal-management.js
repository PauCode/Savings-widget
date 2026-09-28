(() => {
    const goalSelect = document.querySelector("#goal-select");
    const archiveButton = document.querySelector("#archive-goal-button");
    const deleteButton = document.querySelector("#delete-goal-button");
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

    archiveButton.addEventListener("click", () => {
        if (activeGoalId) {
            postMessage({ type: "archiveGoal", id: activeGoalId });
        }
    });

    deleteButton.addEventListener("click", () => {
        if (activeGoalId && window.confirm(`Delete "${activeGoalName}" permanently?`)) {
            postMessage({ type: "deleteGoal", id: activeGoalId });
        }
    });

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
        render({ goals, activeGoalId: selectedId, archivedGoals, canArchiveActiveGoal }) {
            activeGoalId = selectedId;
            activeGoalName = goals.find((goal) => goal.id === selectedId)?.name || "this jar";

            goalSelect.replaceChildren();
            goals.forEach((goal) => {
                const option = document.createElement("option");
                option.value = goal.id;
                option.textContent = goal.name;
                goalSelect.append(option);
            });
            goalSelect.value = selectedId;

            const hasAlternative = goals.length > 1;
            deleteButton.disabled = !hasAlternative;
            archiveButton.disabled = !canArchiveActiveGoal;
            renderArchivedGoals(archivedGoals);
        }
    };
})();
