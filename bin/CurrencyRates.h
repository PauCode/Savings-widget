#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

class CurrencyRates {
public:
    bool FetchLatestRates();
    std::optional<double> Convert(
        double amount, std::string_view fromCode,
        std::string_view toCode) const;
    const std::string& GetRateDate() const noexcept;

    static const std::array<std::string_view, 6>& SupportedCurrencies() noexcept;

private:
    std::unordered_map<std::string, double> usdRates_;
    std::string rateDate_;
};