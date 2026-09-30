(() => {
    const openButton = document.querySelector("#show-rates-button");
    const dialog = document.querySelector("#rates-dialog");
    const baseSelect = document.querySelector("#rates-base");
    const list = document.querySelector("#rates-list");
    const updated = document.querySelector("#rates-updated");
    const refreshButton = document.querySelector("#refresh-rates-dialog");
    const closeButton = document.querySelector("#close-rates-button");

    let rates = {};
    let rateDate = "";
    let selectedCurrency = "";

    function formatRate(value) {
        const digits = value >= 100 ? 2 : value >= 1 ? 4 : 6;
        return value.toLocaleString(undefined, {
            minimumFractionDigits: digits,
            maximumFractionDigits: digits
        });
    }

    function setBaseOptions() {
        const codes = Object.keys(rates);
        const previous = baseSelect.value;
        baseSelect.replaceChildren();
        codes.forEach((code) => {
            const option = document.createElement("option");
            option.value = code;
            option.textContent = code;
            baseSelect.append(option);
        });
        baseSelect.value = codes.includes(previous)
            ? previous
            : codes.includes(selectedCurrency) ? selectedCurrency : codes[0] || "";
    }

    function renderRates() {
        list.replaceChildren();
        const base = baseSelect.value;
        const baseRate = rates[base];

        if (!baseRate) {
            updated.textContent = "Exchange rates are unavailable.";
            return;
        }

        Object.keys(rates)
            .filter((code) => code !== base)
            .forEach((code) => {
                const item = document.createElement("li");
                item.className = "rates-item";

                const pair = document.createElement("span");
                pair.className = "rates-pair";
                pair.textContent = `1 ${base} →`;

                const value = document.createElement("strong");
                value.className = "rates-value";
                value.textContent = `${formatRate(rates[code] / baseRate)} ${code}`;

                item.append(pair, value);
                list.append(item);
            });

        updated.textContent = rateDate
            ? `Rates from ${rateDate}`
            : "Rates unavailable";
    }

    openButton.addEventListener("click", () => {
        setBaseOptions();
        renderRates();
        dialog.showModal();
    });

    closeButton.addEventListener("click", () => dialog.close());
    baseSelect.addEventListener("change", renderRates);

    refreshButton.addEventListener("click", () => {
        window.chrome?.webview?.postMessage({ type: "refreshRates" });
    });

    window.chrome?.webview?.addEventListener("message", (event) => {
        if (event.data?.type !== "state") {
            return;
        }

        rates = event.data.rates || {};
        rateDate = event.data.rateDate || "";
        selectedCurrency = event.data.currency || "";
        openButton.disabled = Object.keys(rates).length === 0;
        refreshButton.disabled = Boolean(event.data.ratesRefreshing);

        if (dialog.open) {
            setBaseOptions();
            renderRates();
        }
    });
})();
