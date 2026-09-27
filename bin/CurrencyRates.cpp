#define _SILENCE_EXPERIMENTAL_COROUTINE_DEPRECATION_WARNINGS

#include "CurrencyRates.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <utility>
#include <windows.h>
#include <winhttp.h>

#include <winrt/Windows.Data.Json.h>

#pragma comment(lib, "Winhttp.lib")
#pragma comment(lib, "WindowsApp.lib")

namespace {
using RateMap = std::unordered_map<std::string, double>;

constexpr std::array<std::string_view, 6> kSupportedCurrencies{
    "USD", "RON", "EUR", "CAD", "RUB", "DKK"
};

constexpr wchar_t kUserAgent[] = L"MoneySavingWidget/1.0";
constexpr std::size_t kMaximumResponseBytes = 1024 * 1024;

struct RateEndpoint {
    const wchar_t* host;
    const wchar_t* path;
};

constexpr RateEndpoint kEndpoints[]{
    {
        L"cdn.jsdelivr.net",
        L"/npm/@fawazahmed0/currency-api@latest/v1/currencies/usd.json"
    },
    {
        L"latest.currency-api.pages.dev",
        L"/v1/currencies/usd.json"
    }
};

class WinHttpHandle {
public:
    explicit WinHttpHandle(HINTERNET handle) noexcept : handle_(handle) {}

    ~WinHttpHandle() {
        if (handle_) {
            WinHttpCloseHandle(handle_);
        }
    }

    WinHttpHandle(const WinHttpHandle&) = delete;
    WinHttpHandle& operator=(const WinHttpHandle&) = delete;

    HINTERNET Get() const noexcept {
        return handle_;
    }

private:
    HINTERNET handle_;
};

class WinRtApartment {
public:
    WinRtApartment() {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
    }

    ~WinRtApartment() {
        winrt::uninit_apartment();
    }

    WinRtApartment(const WinRtApartment&) = delete;
    WinRtApartment& operator=(const WinRtApartment&) = delete;
};

std::optional<std::string> DownloadRates(const RateEndpoint& endpoint) {
    WinHttpHandle session{WinHttpOpen(
        kUserAgent, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)};
    if (!session.Get()) {
        return std::nullopt;
    }
    WinHttpSetTimeouts(session.Get(), 5000, 5000, 5000, 10000);

    WinHttpHandle connection{WinHttpConnect(
        session.Get(), endpoint.host, INTERNET_DEFAULT_HTTPS_PORT, 0)};
    if (!connection.Get()) {
        return std::nullopt;
    }
    WinHttpHandle request{WinHttpOpenRequest(
        connection.Get(), L"GET", endpoint.path, nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE)};
    if (!request.Get() ||
        !WinHttpSendRequest(request.Get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.Get(), nullptr)) {
        return std::nullopt;
    }
    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    if (!WinHttpQueryHeaders(
            request.Get(),
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize,
            WINHTTP_NO_HEADER_INDEX) ||
        statusCode != HTTP_STATUS_OK) {
        return std::nullopt;
    }
    std::string response;
    char buffer[8192];
    for (;;) {
        DWORD bytesRead = 0;
        if (!WinHttpReadData(request.Get(), buffer, sizeof(buffer), &bytesRead)) {
            return std::nullopt;
        }
        if (bytesRead == 0) {
            break;
        }
        if (response.size() + bytesRead > kMaximumResponseBytes) {
            return std::nullopt;
        }
        response.append(buffer, bytesRead);
    }

    return response;
}

std::optional<std::wstring> DecodeUtf8(const std::string& text) {
    if (text.empty()) {
        return std::nullopt;
    }

    const int inputLength = static_cast<int>(text.size());
    const int outputLength = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), inputLength, nullptr, 0);
    if (outputLength == 0) {
        return std::nullopt;
    }

    std::wstring decoded(static_cast<std::size_t>(outputLength), L'\0');
    if (MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), inputLength,
            decoded.data(), outputLength) != outputLength) {
        return std::nullopt;
    }

    return decoded;
}

std::optional<std::string> NormalizeCode(std::string_view code) {
    std::string normalized;
    normalized.reserve(code.size());
    for (unsigned char character : code) {
        normalized.push_back(static_cast<char>(std::toupper(character)));
    }

    const auto found = std::find(
        kSupportedCurrencies.begin(), kSupportedCurrencies.end(), normalized);
    if (found == kSupportedCurrencies.end()) {
        return std::nullopt;
    }

    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return normalized;
}

bool ParseRates(
    const std::string& response, RateMap& rates, std::string& rateDate) {
    const auto decoded = DecodeUtf8(response);
    if (!decoded) {
        return false;
    }

    try {
        const auto document = winrt::Windows::Data::Json::JsonObject::Parse(
            winrt::hstring(*decoded));
        const auto usdRates = document.GetNamedObject(L"usd");
        const auto date = document.GetNamedString(L"date");

        RateMap parsedRates;
        for (const std::string_view code : kSupportedCurrencies) {
            std::wstring key;
            for (unsigned char character : code) {
                key.push_back(static_cast<wchar_t>(std::tolower(character)));
            }
            const double rate = usdRates.GetNamedNumber(key);
            if (!std::isfinite(rate) || rate <= 0.0) {
                return false;
            }
            const auto normalizedCode = NormalizeCode(code);
            if (!normalizedCode) {
                return false;
            }
            parsedRates.emplace(*normalizedCode, rate);
        }

        rates = std::move(parsedRates);
        rateDate = winrt::to_string(date);
        return !rateDate.empty();
    } catch (const winrt::hresult_error&) {
        return false;
    }
}
} // namespace

bool CurrencyRates::FetchLatestRates() {
    try {
        WinRtApartment apartment;
        for (const RateEndpoint& endpoint : kEndpoints) {
            const auto response = DownloadRates(endpoint);
            if (!response) {
                continue;
            }

            RateMap rates;
            std::string rateDate;
            if (ParseRates(*response, rates, rateDate)) {
                usdRates_ = std::move(rates);
                rateDate_ = std::move(rateDate);
                return true;
            }
        }
    } catch (const winrt::hresult_error&) {
        return false;
    }

    return false;
}

std::optional<double> CurrencyRates::Convert(
    double amount, std::string_view fromCode, std::string_view toCode) const {
    if (!std::isfinite(amount) || amount < 0.0) {
        return std::nullopt;
    }

    const auto from = NormalizeCode(fromCode);
    const auto to = NormalizeCode(toCode);
    if (!from || !to) {
        return std::nullopt;
    }
    if (*from == *to) {
        return amount;
    }

    const auto fromRate = usdRates_.find(*from);
    const auto toRate = usdRates_.find(*to);
    if (fromRate == usdRates_.end() || toRate == usdRates_.end()) {
        return std::nullopt;
    }

    const double converted = amount * toRate->second / fromRate->second;
    return std::isfinite(converted)
        ? std::optional<double>{converted}
        : std::nullopt;
}

const std::string& CurrencyRates::GetRateDate() const noexcept {
    return rateDate_;
}

const std::array<std::string_view, 6>&
CurrencyRates::SupportedCurrencies() noexcept {
    return kSupportedCurrencies;
}