#include "SavingsData.h"

#include <cmath>
#include <cstdint>
#include <fstream>

namespace {
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
    if (value > static_cast<std::uint32_t>(maximumSavedCents)) {
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
} // namespace

bool SavingsData::Load(const std::filesystem::path& directory) {
    directory_ = directory;
    if (directory_.empty()) {
        return false;
    }

    const auto path = directory_ / L"savings.dat";
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error) {
        return false;
    }
    if (!exists) {
        savedCents_ = 0;
        return true;
    }

    return ReadBinaryBalance(path, kMaximumSavedCents, savedCents_);
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

bool SavingsData::Save(int savedCents) const {
    return WriteBinaryBalance(directory_ / L"savings.dat", savedCents);
}