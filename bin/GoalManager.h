#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "SavingsData.h"

struct GoalInfo {
    std::wstring id;
    std::wstring name;
};

struct GoalHistoryEntry {
    long long timestampMillis = 0;
    std::wstring type; // "deposit" | "withdraw" | "reset"
    int amountRonCents = 0;
    std::wstring note;
};

// Owns the list of savings goals ("jars"), the currently active goal, and
// the balance/target/history for that active goal. Every goal is backed by
// its own directory under %APPDATA%/PauCode/Savings Jar/goals/<id>/ containing:
//   savings.dat  - 4-byte balance in RON cents (SavingsData format)
//   goal.dat     - 4-byte target in RON cents
//   history.log  - append-only "<millis>|<type>|<ronCents>|<note>" text lines
// The goals/index.txt file tracks the active goal id and the goal list.
// A first run creates an empty index; the UI then asks the user to create
// their first jar.
class GoalManager {
public:
    bool Load();

    const std::vector<GoalInfo>& GetGoals() const noexcept;
    const std::vector<GoalInfo>& GetArchivedGoals() const noexcept;
    const std::wstring& GetActiveGoalId() const noexcept;
    const std::wstring& GetActiveGoalName() const noexcept;

    bool SelectGoal(const std::wstring& id);
    bool CreateGoal(const std::wstring& name, int targetRonCents, std::wstring& newId);
    bool RenameGoal(const std::wstring& id, const std::wstring& name);
    bool DeleteGoal(const std::wstring& id);
    bool ArchiveGoal(const std::wstring& id);

    int GetActiveGoalTargetRonCents() const noexcept;
    bool SetActiveGoalTargetRonCents(int targetRonCents);

    int GetActiveSavedCents() const noexcept;
    DepositResult AddDeposit(double amountRon, const std::wstring& note = {});
    DepositResult RemoveFunds(double amountRon, const std::wstring& note = {});
    bool ResetActiveGoal();

    // Older goal directories stored the balance in USD cents (converted to
    // RON only for display). These two let the caller detect that case and
    // rewrite the balance in RON cents using a caller-supplied conversion,
    // exactly once per goal directory.
    bool ActiveGoalNeedsLegacyUnitMigration() const;
    bool MigrateActiveGoalBalanceToRon(double convertedRonAmount);

    std::vector<GoalHistoryEntry> GetActiveHistory(std::size_t maxEntries) const;

private:
    bool EnsureMigrated();
    bool LoadIndex();
    bool SaveIndex() const;
    bool LoadArchivedIndex();
    bool SaveArchivedIndex() const;
    bool LoadActiveGoalData(const std::wstring& id);
    bool LoadGoalTarget(const std::wstring& id, int& targetRonCents) const;
    bool SaveGoalTarget(const std::wstring& id, int targetRonCents) const;
    void RecordHistory(
        const wchar_t* type, int amountRonCents, const std::wstring& note) const;
    std::wstring MakeGoalId() const;

    std::vector<GoalInfo> goals_;
    std::vector<GoalInfo> archivedGoals_;
    std::wstring activeId_;
    std::wstring activeName_;
    int activeTargetRonCents_ = 0;
    SavingsData activeSavings_;
};
