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
    static constexpr int kDefaultGoalCents = 100000;
    static constexpr int kMaximumGoalCents = 100000000;

    bool Load();
    int GetSavedCents() const noexcept;
    DepositResult AddDeposit(double amount);
    DepositResult RemoveFunds(double amount);
    bool Reset();
    bool LoadGoalCents(int& goalUsdCents, int& displayRonCents) const;
    bool SaveGoalCents(int goalUsdCents, int displayRonCents) const;

private:
    static constexpr int kMaximumSavedCents = kMaximumGoalCents;

    bool Save(int savedCents) const;

    int savedCents_ = 0;
};