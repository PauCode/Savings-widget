(() => {
    const dialog = document.querySelector("#close-behavior-dialog");
    const rememberChoice = document.querySelector("#remember-close-choice");
    const closeButton = document.querySelector("#close-app-button");
    const minimizeButton = document.querySelector("#minimize-app-button");

    function choose(action) {
        window.chrome?.webview?.postMessage({
            type: "closeBehavior",
            action,
            remember: rememberChoice.checked
        });
        dialog.close();
    }

    window.chrome?.webview?.addEventListener("message", (event) => {
        if (event.data?.type !== "closeRequested") {
            return;
        }
        rememberChoice.checked = false;
        if (!dialog.open) {
            dialog.showModal();
        }
    });

    dialog.addEventListener("cancel", (event) => event.preventDefault());
    closeButton.addEventListener("click", () => choose("close"));
    minimizeButton.addEventListener("click", () => choose("minimize"));
})();
