#include "GoalManager.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <system_error>

#include <windows.h>

namespace {
namespace fs = std::filesystem;

fs::path GetWorkspaceRoot() {
    wchar_t exePath[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return {};
    }
    return fs::path(exePath).parent_path().parent_path().parent_path();
}

fs::path GetGoalsRoot() {
    const auto root = GetWorkspaceRoot();
    return root.empty() ? fs::path{} : root / L"current" / L"goals";
}

fs::path GetGoalDirectory(const std::wstring& id) {
    const auto root = GetGoalsRoot();
    return root.empty() || id.empty() ? fs::path{} : root / id;
}

std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(
        CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (length <= 0) {
        return {};
    }
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(
        CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), length);
    return wide;
}

std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }
    const int length = WideCharToMultiByte(
        CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0,
        nullptr, nullptr);
    if (length <= 0) {
        return {};
    }
    std::string narrow(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, text.data(), static_cast<int>(text.size()), narrow.data(),
        length, nullptr, nullptr);
    return narrow;
}

std::wstring SanitizeGoalName(const std::wstring& rawName) {
    std::wstring name;
    name.reserve(rawName.size());
    for (wchar_t character : rawName) {
        if (character == L'|' || character == L'\n' || character == L'\r') {
            name.push_back(L' ');
        } else {
            name.push_back(character);
        }
    }

    const auto isSpace = [](wchar_t character) {
        return character == L' ' || character == L'\t';
    };
    while (!name.empty() && isSpace(name.front())) {
        name.erase(name.begin());
    }
    while (!name.empty() && isSpace(name.back())) {
        name.pop_back();
    }
    if (name.size() > 60) {
        name.resize(60);
    }
    return name;
}

bool ReadUint32(const fs::path& path, std::uint32_t& value) {
    std::ifstream file(path, std::ios::binary);
    unsigned char bytes[4]{};
    file.read(reinterpret_cast<char*>(bytes), sizeof(bytes));
    if (!file || file.peek() != std::char_traits<char>::eof()) {
        return false;
    }
    value = static_cast<std::uint32_t>(bytes[0]) |
        (static_cast<std::uint32_t>(bytes[1]) << 8) |
        (static_cast<std::uint32_t>(bytes[2]) << 16) |
        (static_cast<std::uint32_t>(bytes[3]) << 24);
    return true;
}

bool WriteUint32(const fs::path& path, std::uint32_t value) {
    std::error_code error;
    fs::create_directories(path.parent_path(), error);
    if (error) {
        return false;
    }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    const unsigned char bytes[4]{
        static_cast<unsigned char>(value & 0xff),
        static_cast<unsigned char>((value >> 8) & 0xff),
        static_cast<unsigned char>((value >> 16) & 0xff),
        static_cast<unsigned char>((value >> 24) & 0xff)
    };
    file.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
    return file.good();
}

long long NowMillis() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
}
} // namespace

bool GoalManager::Load() {
    if (!EnsureMigrated()) {
        return false;
    }
    if (!LoadIndex()) {
        return false;
    }
    return LoadActiveGoalData(activeId_);
}

const std::vector<GoalInfo>& GoalManager::GetGoals() const noexcept {
    return goals_;
}

const std::wstring& GoalManager::GetActiveGoalId() const noexcept {
    return activeId_;
}

const std::wstring& GoalManager::GetActiveGoalName() const noexcept {
    return activeName_;
}

bool GoalManager::SelectGoal(const std::wstring& id) {
    if (std::none_of(goals_.begin(), goals_.end(),
                     [&id](const GoalInfo& goal) { return goal.id == id; })) {
        return false;
    }
    if (!LoadActiveGoalData(id)) {
        return false;
    }
    activeId_ = id;
    return SaveIndex();
}

bool GoalManager::CreateGoal(
    const std::wstring& rawName, int targetRonCents, std::wstring& newId) {
    if (targetRonCents <= 0 ||
        targetRonCents > SavingsData::kMaximumSavedCents) {
        return false;
    }

    std::wstring name = SanitizeGoalName(rawName);
    if (name.empty()) {
        name = L"Savings Goal";
    }

    newId = MakeGoalId();
    const auto directory = GetGoalDirectory(newId);
    if (directory.empty()) {
        return false;
    }

    std::error_code error;
    fs::create_directories(directory, error);
    if (error) {
        return false;
    }

    SavingsData savings;
    if (!savings.Load(directory) || !SaveGoalTarget(newId, targetRonCents)) {
        return false;
    }

    goals_.push_back({newId, name});
    activeId_ = newId;
    if (!SaveIndex() || !LoadActiveGoalData(newId)) {
        return false;
    }
    return true;
}

int GoalManager::GetActiveGoalTargetRonCents() const noexcept {
    return activeTargetRonCents_;
}

bool GoalManager::SetActiveGoalTargetRonCents(int targetRonCents) {
    if (targetRonCents <= 0 ||
        targetRonCents > SavingsData::kMaximumSavedCents ||
        activeId_.empty()) {
        return false;
    }
    if (!SaveGoalTarget(activeId_, targetRonCents)) {
        return false;
    }
    activeTargetRonCents_ = targetRonCents;
    return true;
}

int GoalManager::GetActiveSavedCents() const noexcept {
    return activeSavings_.GetSavedCents();
}

DepositResult GoalManager::AddDeposit(double amountUsd, double amountRon) {
    const DepositResult result = activeSavings_.AddDeposit(amountUsd);
    if (result == DepositResult::Added) {
        RecordHistory(L"deposit", static_cast<int>(std::lround(amountRon * 100.0)));
    }
    return result;
}

DepositResult GoalManager::RemoveFunds(double amountUsd, double amountRon) {
    const DepositResult result = activeSavings_.RemoveFunds(amountUsd);
    if (result == DepositResult::Removed) {
        RecordHistory(L"withdraw", static_cast<int>(std::lround(amountRon * 100.0)));
    }
    return result;
}

bool GoalManager::ResetActiveGoal() {
    if (!activeSavings_.Reset()) {
        return false;
    }
    RecordHistory(L"reset", 0);
    return true;
}

std::vector<GoalHistoryEntry> GoalManager::GetActiveHistory(
    std::size_t maxEntries) const {
    std::vector<GoalHistoryEntry> entries;
    const auto directory = GetGoalDirectory(activeId_);
    if (directory.empty()) {
        return entries;
    }

    std::ifstream file(directory / L"history.log");
    if (!file) {
        return entries;
    }

    std::string line;
    while (std::getline(file, line)) {
        const auto firstSeparator = line.find('|');
        if (firstSeparator == std::string::npos) {
            continue;
        }
        const auto secondSeparator = line.find('|', firstSeparator + 1);
        if (secondSeparator == std::string::npos) {
            continue;
        }

        GoalHistoryEntry entry{};
        try {
            entry.timestampMillis = std::stoll(line.substr(0, firstSeparator));
            const std::string type = line.substr(
                firstSeparator + 1, secondSeparator - firstSeparator - 1);
            entry.type = Utf8ToWide(type);
            entry.amountRonCents = std::stoi(line.substr(secondSeparator + 1));
        } catch (const std::exception&) {
            continue;
        }
        entries.push_back(entry);
    }

    if (entries.size() > maxEntries) {
        entries.erase(
            entries.begin(), entries.end() - static_cast<std::ptrdiff_t>(maxEntries));
    }
    std::reverse(entries.begin(), entries.end());
    return entries;
}

bool GoalManager::EnsureMigrated() {
    const auto goalsRoot = GetGoalsRoot();
    if (goalsRoot.empty()) {
        return false;
    }

    std::error_code error;
    const auto indexPath = goalsRoot / L"index.txt";
    if (fs::exists(indexPath, error)) {
        return !error;
    }
    if (error) {
        return false;
    }

    const auto workspaceRoot = GetWorkspaceRoot();

    int legacySavedCents = 0;
    const auto legacyBinaryPath = workspaceRoot / L"current" / L"savings.dat";
    std::uint32_t legacyValue = 0;
    std::error_code legacyError;
    if (fs::exists(legacyBinaryPath, legacyError) && !legacyError &&
        ReadUint32(legacyBinaryPath, legacyValue) &&
        legacyValue <= static_cast<std::uint32_t>(SavingsData::kMaximumSavedCents)) {
        legacySavedCents = static_cast<int>(legacyValue);
    } else {
        const auto legacyTextPath = workspaceRoot / L"Data" / L"savings.txt";
        std::ifstream legacyTextFile(legacyTextPath);
        long long legacyTextValue = 0;
        if (legacyTextFile && (legacyTextFile >> legacyTextValue) &&
            legacyTextValue >= 0 &&
            legacyTextValue <= SavingsData::kMaximumSavedCents) {
            legacySavedCents = static_cast<int>(legacyTextValue);
        }
    }

    int legacyTargetRonCents = 0;
    const auto legacyGoalPath = workspaceRoot / L"current" / L"goal.dat";
    std::error_code goalSizeError;
    const auto legacyGoalSize = fs::file_size(legacyGoalPath, goalSizeError);
    if (!goalSizeError && legacyGoalSize >= 8) {
        std::ifstream legacyGoalFile(legacyGoalPath, std::ios::binary);
        unsigned char bytes[8]{};
        legacyGoalFile.read(reinterpret_cast<char*>(bytes), sizeof(bytes));
        if (legacyGoalFile) {
            const std::uint32_t ronCents =
                static_cast<std::uint32_t>(bytes[4]) |
                (static_cast<std::uint32_t>(bytes[5]) << 8) |
                (static_cast<std::uint32_t>(bytes[6]) << 16) |
                (static_cast<std::uint32_t>(bytes[7]) << 24);
            if (ronCents > 0 &&
                ronCents <= static_cast<std::uint32_t>(SavingsData::kMaximumSavedCents)) {
                legacyTargetRonCents = static_cast<int>(ronCents);
            }
        }
    }

    constexpr wchar_t kDefaultId[] = L"default";
    const auto defaultDirectory = goalsRoot / kDefaultId;
    fs::create_directories(defaultDirectory, error);
    if (error) {
        return false;
    }

    SavingsData savings;
    if (!savings.Load(defaultDirectory)) {
        return false;
    }
    if (legacySavedCents > 0 && savings.AddDeposit(legacySavedCents / 100.0) !=
        DepositResult::Added) {
        return false;
    }

    if (legacyTargetRonCents > 0 &&
        !WriteUint32(defaultDirectory / L"goal.dat",
                     static_cast<std::uint32_t>(legacyTargetRonCents))) {
        return false;
    }

    goals_ = {{kDefaultId, L"Savings Goal"}};
    activeId_ = kDefaultId;
    return SaveIndex();
}

bool GoalManager::LoadIndex() {
    const auto goalsRoot = GetGoalsRoot();
    if (goalsRoot.empty()) {
        return false;
    }

    std::ifstream file(goalsRoot / L"index.txt");
    if (!file) {
        return false;
    }

    std::string activeIdUtf8;
    if (!std::getline(file, activeIdUtf8)) {
        return false;
    }
    activeId_ = Utf8ToWide(activeIdUtf8);

    goals_.clear();
    std::string line;
    while (std::getline(file, line)) {
        const auto separator = line.find('|');
        if (separator == std::string::npos) {
            continue;
        }
        GoalInfo goal;
        goal.id = Utf8ToWide(line.substr(0, separator));
        goal.name = Utf8ToWide(line.substr(separator + 1));
        if (!goal.id.empty()) {
            goals_.push_back(std::move(goal));
        }
    }

    if (goals_.empty() ||
        std::none_of(goals_.begin(), goals_.end(),
                     [this](const GoalInfo& goal) { return goal.id == activeId_; })) {
        return false;
    }
    return true;
}

bool GoalManager::SaveIndex() const {
    const auto goalsRoot = GetGoalsRoot();
    if (goalsRoot.empty()) {
        return false;
    }

    std::error_code error;
    fs::create_directories(goalsRoot, error);
    if (error) {
        return false;
    }

    std::ofstream file(goalsRoot / L"index.txt", std::ios::trunc);
    if (!file) {
        return false;
    }

    file << WideToUtf8(activeId_) << '\n';
    for (const GoalInfo& goal : goals_) {
        file << WideToUtf8(goal.id) << '|' << WideToUtf8(goal.name) << '\n';
    }
    return file.good();
}

bool GoalManager::LoadActiveGoalData(const std::wstring& id) {
    const auto directory = GetGoalDirectory(id);
    if (directory.empty()) {
        return false;
    }

    SavingsData savings;
    if (!savings.Load(directory)) {
        return false;
    }

    int targetRonCents = 0;
    LoadGoalTarget(id, targetRonCents);

    activeSavings_ = std::move(savings);
    activeTargetRonCents_ = targetRonCents;
    activeName_.clear();
    for (const GoalInfo& goal : goals_) {
        if (goal.id == id) {
            activeName_ = goal.name;
            break;
        }
    }
    return true;
}

bool GoalManager::LoadGoalTarget(
    const std::wstring& id, int& targetRonCents) const {
    targetRonCents = 0;
    const auto directory = GetGoalDirectory(id);
    if (directory.empty()) {
        return false;
    }

    const auto path = directory / L"goal.dat";
    std::error_code error;
    if (!fs::exists(path, error) || error) {
        return !error;
    }

    std::uint32_t value = 0;
    if (!ReadUint32(path, value) ||
        value > static_cast<std::uint32_t>(SavingsData::kMaximumSavedCents)) {
        return false;
    }
    targetRonCents = static_cast<int>(value);
    return true;
}

bool GoalManager::SaveGoalTarget(
    const std::wstring& id, int targetRonCents) const {
    const auto directory = GetGoalDirectory(id);
    if (directory.empty()) {
        return false;
    }
    return WriteUint32(
        directory / L"goal.dat", static_cast<std::uint32_t>(targetRonCents));
}

void GoalManager::RecordHistory(const wchar_t* type, int amountRonCents) const {
    const auto directory = GetGoalDirectory(activeId_);
    if (directory.empty()) {
        return;
    }

    std::error_code error;
    fs::create_directories(directory, error);
    if (error) {
        return;
    }

    std::ofstream file(directory / L"history.log", std::ios::app);
    if (!file) {
        return;
    }
    file << NowMillis() << '|' << WideToUtf8(type) << '|' << amountRonCents << '\n';
}

std::wstring GoalManager::MakeGoalId() const {
    return L"goal-" + std::to_wstring(NowMillis());
}
