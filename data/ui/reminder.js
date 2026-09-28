(() => {
    const openButton = document.querySelector("#create-reminder-button");
    const dialog = document.querySelector("#reminder-dialog");
    const form = document.querySelector("#reminder-form");
    const enabledCheckbox = document.querySelector("#reminder-enabled");
    const daySelect = document.querySelector("#reminder-day");
    const defaultCheckbox = document.querySelector("#reminder-default-message");
    const messageInput = document.querySelector("#reminder-message");
    const errorMessage = document.querySelector("#reminder-error");
    const cancelButton = document.querySelector("#cancel-reminder");

    let current = {
        enabled: false,
        day: 1,
        useDefaultMessage: true,
        message: "",
        defaultMessage: ""
    };

    for (let day = 1; day <= 31; day += 1) {
        const option = document.createElement("option");
        option.value = String(day);
        option.textContent = String(day);
        daySelect.append(option);
    }

    function syncMessageField() {
        const useDefault = defaultCheckbox.checked;
        messageInput.disabled = useDefault;
        messageInput.placeholder = current.defaultMessage;
        if (useDefault) {
            messageInput.value = "";
        }
    }

    function syncEnabledState() {
        const active = enabledCheckbox.checked;
        daySelect.disabled = !active;
        defaultCheckbox.disabled = !active;
        messageInput.disabled = !active || defaultCheckbox.checked;
    }

    openButton.addEventListener("click", () => {
        errorMessage.textContent = "";
        enabledCheckbox.checked = current.enabled;
        daySelect.value = String(current.day);
        defaultCheckbox.checked = current.useDefaultMessage;
        messageInput.value = current.useDefaultMessage ? "" : current.message;
        syncMessageField();
        syncEnabledState();
        dialog.showModal();
    });

    cancelButton.addEventListener("click", () => dialog.close());
    defaultCheckbox.addEventListener("change", syncMessageField);
    enabledCheckbox.addEventListener("change", syncEnabledState);

    form.addEventListener("submit", (event) => {
        event.preventDefault();
        const useDefaultMessage = defaultCheckbox.checked;
        const message = messageInput.value.trim();
        if (!useDefaultMessage && !message) {
            errorMessage.textContent = "Enter a message or keep the default one.";
            return;
        }

        window.chrome?.webview?.postMessage({
            type: "reminder",
            enabled: enabledCheckbox.checked,
            day: Number(daySelect.value),
            useDefaultMessage,
            message: useDefaultMessage ? "" : message
        });
        dialog.close();
    });

    window.chrome?.webview?.addEventListener("message", (event) => {
        if (event.data?.type !== "state") {
            return;
        }
        current = {
            enabled: Boolean(event.data.reminderEnabled),
            day: Number(event.data.reminderDay) || 1,
            useDefaultMessage: event.data.reminderUsesDefaultMessage !== false,
            message: event.data.reminderMessage || "",
            defaultMessage: event.data.reminderDefaultMessage || ""
        };
        openButton.textContent = current.enabled
            ? `Notification · day ${current.day}`
            : "Create notification";
        openButton.classList.toggle("is-active", current.enabled);
    });
})();
