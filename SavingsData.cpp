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
        / L"Data" / L"savings.dat";
}

std::filesystem::path GetLegacyDataFilePath() {
    const auto binaryPath = GetBinaryDataFilePath();
    return binaryPath.empty()
        ? std::filesystem::path{}
        : binaryPath.parent_path() / L"savings.txt";
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

bool SavingsData::Save(int savedCents) const {
    const auto path = GetBinaryDataFilePath();
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