#pragma once

enum class DepositResult {
    Added,
    Removed,
    InvalidAmount,
    TooLarge,
    InsufficientFunds,
    SaveFailed
};

class SavingsData {
public:
    bool Load();
    int GetSavedCents() const noexcept;
    DepositResult AddDeposit(double amount);
    DepositResult RemoveFunds(double amount);
    bool Reset();

private:
    static constexpr int kMaximumSavedCents = 100000000;

    bool Save(int savedCents) const;

    int savedCents_ = 0;
};