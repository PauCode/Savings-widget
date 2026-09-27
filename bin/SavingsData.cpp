#include "SavingsData.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <windows.h>

namespace {
std::filesystem::path GetBinaryDataFilePath() {
    wchar_t exePath[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return {};
    }

    return std::filesystem::path(exePath).parent_path()
        .parent_path().parent_path()
        / L"current" / L"savings.dat";
}

std::filesystem::path GetLegacyDataFilePath() {
    const auto binaryPath = GetBinaryDataFilePath();
    return binaryPath.empty()
        ? std::filesystem::path{}
        : binaryPath.parent_path().parent_path()
            / L"Data" / L"savings.txt";
}

std::filesystem::path GetGoalDataFilePath() {
    const auto balancePath = GetBinaryDataFilePath();
    return balancePath.empty()
        ? std::filesystem::path{}
        : balancePath.parent_path() / L"goal.dat";
}

bool ReadBinaryBalance(
    const std::filesystem::path& path, int maximumSavedCents, int& savedCents) {
    std::ifstream file(path, std::ios::binary);
    unsigned char bytes[4]{};
    file.read(reinterpret_cast<char*>(bytes), sizeof(bytes));
    if (!file || file.peek() != std::char_traits<char>::eof()) {
        return false;
    }

    const std::uint32_t value =
        static_cast<std::uint32_t>(bytes[0]) |
        (static_cast<std::uint32_t>(bytes[1]) << 8) |
        (static_cast<std::uint32_t>(bytes[2]) << 16) |
        (static_cast<std::uint32_t>(bytes[3]) << 24);
    if (value > maximumSavedCents) {
        return false;
    }

    savedCents = static_cast<int>(value);
    return true;
}

bool WriteBinaryBalance(const std::filesystem::path& path, int savedCents) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        return false;
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }

    const std::uint32_t value = static_cast<std::uint32_t>(savedCents);
    const unsigned char bytes[4]{
        static_cast<unsigned char>(value & 0xff),
        static_cast<unsigned char>((value >> 8) & 0xff),
        static_cast<unsigned char>((value >> 16) & 0xff),
        static_cast<unsigned char>((value >> 24) & 0xff)
    };
    file.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
    return file.good();
}

bool ReadLegacyBalance(
    const std::filesystem::path& path, int maximumSavedCents, int& savedCents) {
    std::ifstream file(path);
    long long value = 0;
    if (!file || !(file >> value) ||
        value < 0 || value > maximumSavedCents) {
        return false;
    }

    savedCents = static_cast<int>(value);
    return true;
}

void RemoveLegacyDataFile(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::remove(path, error);
}
} // namespace

bool SavingsData::Load() {
    const auto binaryPath = GetBinaryDataFilePath();
    const auto legacyPath = GetLegacyDataFilePath();
    if (binaryPath.empty() || legacyPath.empty()) {
        return false;
    }

    std::error_code error;
    const bool binaryExists = std::filesystem::exists(binaryPath, error);
    if (error) {
        return false;
    }
    if (binaryExists) {
        if (!ReadBinaryBalance(binaryPath, kMaximumSavedCents, savedCents_)) {
            return false;
        }
        RemoveLegacyDataFile(legacyPath);
        return true;
    }

    const bool legacyExists = std::filesystem::exists(legacyPath, error);
    if (error) {
        return false;
    }
    if (!legacyExists) {
        savedCents_ = 0;
        return true;
    }

    int legacySavedCents = 0;
    if (!ReadLegacyBalance(legacyPath, kMaximumSavedCents, legacySavedCents) ||
        !Save(legacySavedCents)) {
        return false;
    }

    savedCents_ = legacySavedCents;
    RemoveLegacyDataFile(legacyPath);
    return true;
}

int SavingsData::GetSavedCents() const noexcept {
    return savedCents_;
}

DepositResult SavingsData::AddDeposit(double amount) {
    if (!std::isfinite(amount) || amount <= 0.0) {
        return DepositResult::InvalidAmount;
    }
    if (amount > 1000000.0) {
        return DepositResult::TooLarge;
    }

    const int depositCents = static_cast<int>(std::lround(amount * 100.0));
    if (depositCents <= 0) {
        return DepositResult::InvalidAmount;
    }
    if (depositCents > kMaximumSavedCents - savedCents_) {
        return DepositResult::TooLarge;
    }

    const int newSavedCents = savedCents_ + depositCents;
    if (!Save(newSavedCents)) {
        return DepositResult::SaveFailed;
    }

    savedCents_ = newSavedCents;
    return DepositResult::Added;
}

DepositResult SavingsData::RemoveFunds(double amount) {
    if (!std::isfinite(amount) || amount <= 0.0) {
        return DepositResult::InvalidAmount;
    }
    if (amount > 1000000.0) {
        return DepositResult::TooLarge;
    }

    const int amountCents = static_cast<int>(std::lround(amount * 100.0));
    if (amountCents <= 0) {
        return DepositResult::InvalidAmount;
    }
    if (amountCents > savedCents_) {
        return DepositResult::InsufficientFunds;
    }

    const int newSavedCents = savedCents_ - amountCents;
    if (!Save(newSavedCents)) {
        return DepositResult::SaveFailed;
    }

    savedCents_ = newSavedCents;
    return DepositResult::Removed;
}

bool SavingsData::Reset() {
    if (!Save(0)) {
        return false;
    }

    savedCents_ = 0;
    return true;
}

bool SavingsData::LoadGoalCents(
    int& goalUsdCents, int& displayRonCents) const {
    goalUsdCents = kDefaultGoalCents;
    displayRonCents = 0;
    const auto path = GetGoalDataFilePath();
    if (path.empty()) {
        return false;
    }

    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error) {
        return false;
    }
    if (!exists) {
        return true;
    }

    std::ifstream file(path, std::ios::binary);
    unsigned char bytes[8]{};
    file.read(reinterpret_cast<char*>(bytes), 4);
    if (!file) {
        return false;
    }

    const auto decodeCents = [&bytes](std::size_t offset) {
        return static_cast<std::uint32_t>(bytes[offset]) |
            (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
            (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
            (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
    };

    const std::uint32_t storedUsdCents = decodeCents(0);
    if (storedUsdCents == 0 || storedUsdCents > kMaximumSavedCents) {
        return false;
    }
    goalUsdCents = static_cast<int>(storedUsdCents);

    if (file.peek() != std::char_traits<char>::eof()) {
        file.read(reinterpret_cast<char*>(bytes + 4), 4);
        if (!file || file.peek() != std::char_traits<char>::eof()) {
            return false;
        }
        const std::uint32_t storedRonCents = decodeCents(4);
        if (storedRonCents == 0 || storedRonCents > kMaximumSavedCents) {
            return false;
        }
        displayRonCents = static_cast<int>(storedRonCents);
    }

    return true;
}

bool SavingsData::SaveGoalCents(
    int goalUsdCents, int displayRonCents) const {
    if (goalUsdCents <= 0 || goalUsdCents > kMaximumSavedCents ||
        displayRonCents <= 0 || displayRonCents > kMaximumSavedCents) {
        return false;
    }

    const auto path = GetGoalDataFilePath();
    if (path.empty()) {
        return false;
    }

    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        return false;
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }

    const std::uint32_t values[2]{
        static_cast<std::uint32_t>(goalUsdCents),
        static_cast<std::uint32_t>(displayRonCents)
    };
    unsigned char bytes[8]{};
    for (std::size_t valueIndex = 0; valueIndex < 2; ++valueIndex) {
        const std::uint32_t value = values[valueIndex];
        const std::size_t offset = valueIndex * 4;
        bytes[offset] = static_cast<unsigned char>(value & 0xff);
        bytes[offset + 1] = static_cast<unsigned char>((value >> 8) & 0xff);
        bytes[offset + 2] = static_cast<unsigned char>((value >> 16) & 0xff);
        bytes[offset + 3] = static_cast<unsigned char>((value >> 24) & 0xff);
    }
    file.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
    return file.good();
}

bool SavingsData::Save(int savedCents) const {
    const auto path = GetBinaryDataFilePath();
    return !path.empty() && WriteBinaryBalance(path, savedCents);
}