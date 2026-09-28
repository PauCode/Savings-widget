(() => {
    const dialog = document.querySelector("#startup-settings-dialog");
    const enableButton = document.querySelector("#enable-startup-button");
    const disableButton = document.querySelector("#disable-startup-button");

    function choose(enabled) {
        window.chrome?.webview?.postMessage({ type: "startupChoice", enabled });
        dialog.close();
    }

    window.chrome?.webview?.addEventListener("message", (event) => {
        if (event.data?.type !== "state" || !event.data.startupChoiceRequired) {
            return;
        }
        if (!dialog.open) {
            dialog.showModal();
        }
    });

    dialog.addEventListener("cancel", (event) => event.preventDefault());
    enableButton.addEventListener("click", () => choose(true));
    disableButton.addEventListener("click", () => choose(false));
})();
