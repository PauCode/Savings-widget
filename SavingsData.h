#pragma once

enum class DepositResult {
    Added,
    InvalidAmount,
    TooLarge,
    SaveFailed
};

class SavingsData {
public:
    bool Load();
    int GetSavedCents() const noexcept;
    DepositResult AddDeposit(double amount);

private:
    static constexpr int kMaximumSavedCents = 100000000;

    bool Save(int savedCents) const;

    int savedCents_ = 0;
};