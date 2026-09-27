#pragma once

#include <filesystem>

enum class DepositResult {
    Added,
    Removed,
    InvalidAmount,
    TooLarge,
    InsufficientFunds,
    SaveFailed
};

// Stores a single balance (in cents) inside a given directory as
// "savings.dat". Each savings goal owns its own SavingsData instance
// pointed at its own goal directory (see GoalManager).
class SavingsData {
public:
    static constexpr int kMaximumSavedCents = 100000000;

    bool Load(const std::filesystem::path& directory);
    int GetSavedCents() const noexcept;
    DepositResult AddDeposit(double amount);
    DepositResult RemoveFunds(double amount);
    bool Reset();

private:
    bool Save(int savedCents) const;

    std::filesystem::path directory_;
    int savedCents_ = 0;
};