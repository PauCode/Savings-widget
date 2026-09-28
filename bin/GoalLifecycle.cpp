#include "GoalManager.h"
#include "AppPaths.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <system_error>

#include <windows.h>

namespace {
namespace fs = std::filesystem;

fs::path GetGoalsRoot() {
    const auto dataDirectory = AppPaths::DataDirectory();
    return dataDirectory.empty() ? fs::path{} : dataDirectory / L"goals";
}

fs::path GetArchivedRoot() {
    const auto goalsRoot = GetGoalsRoot();
    return goalsRoot.empty() ? fs::path{} : goalsRoot / L"archived";
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
        name.push_back(character == L'|' || character == L'\n' || character == L'\r'
            ? L' '
            : character);
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
    if (name.size() > 30) {
        name.resize(30);
    }
    return name;
}
} // namespace

const std::vector<GoalInfo>& GoalManager::GetArchivedGoals() const noexcept {
    return archivedGoals_;
}

bool GoalManager::RenameGoal(const std::wstring& id, const std::wstring& rawName) {
    const auto goal = std::find_if(
        goals_.begin(), goals_.end(),
        [&id](const GoalInfo& candidate) { return candidate.id == id; });
    const std::wstring name = SanitizeGoalName(rawName);
    if (goal == goals_.end() || name.empty()) {
        return false;
    }

    const std::wstring previousName = goal->name;
    goal->name = name;
    if (id == activeId_) {
        activeName_ = name;
    }
    if (!SaveIndex()) {
        goal->name = previousName;
        if (id == activeId_) {
            activeName_ = previousName;
        }
        return false;
    }
    return true;
}

bool GoalManager::DeleteGoal(const std::wstring& id) {
    const auto goal = std::find_if(
        goals_.begin(), goals_.end(),
        [&id](const GoalInfo& candidate) { return candidate.id == id; });
    if (goal == goals_.end() || goals_.size() <= 1) {
        return false;
    }

    const auto goalsRoot = GetGoalsRoot();
    if (goalsRoot.empty()) {
        return false;
    }

    if (id == activeId_) {
        const auto replacement = std::find_if(
            goals_.begin(), goals_.end(),
            [&id](const GoalInfo& candidate) { return candidate.id != id; });
        if (replacement == goals_.end() || !LoadActiveGoalData(replacement->id)) {
            return false;
        }
        activeId_ = replacement->id;
    }

    goals_.erase(goal);
    if (!SaveIndex()) {
        return false;
    }

    std::error_code error;
    fs::remove_all(goalsRoot / id, error);
    return !error;
}

bool GoalManager::ArchiveGoal(const std::wstring& id) {
    const auto goal = std::find_if(
        goals_.begin(), goals_.end(),
        [&id](const GoalInfo& candidate) { return candidate.id == id; });
    if (goal == goals_.end() || goals_.size() <= 1) {
        return false;
    }

    const auto goalsRoot = GetGoalsRoot();
    const auto archivedRoot = GetArchivedRoot();
    if (goalsRoot.empty() || archivedRoot.empty()) {
        return false;
    }

    int targetRonCents = 0;
    SavingsData savings;
    if (!LoadGoalTarget(id, targetRonCents) || targetRonCents <= 0 ||
        !savings.Load(goalsRoot / id) ||
        savings.GetSavedCents() < targetRonCents) {
        return false;
    }

    std::error_code error;
    fs::create_directories(archivedRoot, error);
    if (error || fs::exists(archivedRoot / id, error)) {
        return false;
    }

    const GoalInfo archivedGoal = *goal;
    fs::rename(goalsRoot / id, archivedRoot / id, error);
    if (error) {
        return false;
    }

    if (id == activeId_) {
        const auto replacement = std::find_if(
            goals_.begin(), goals_.end(),
            [&id](const GoalInfo& candidate) { return candidate.id != id; });
        if (replacement == goals_.end() || !LoadActiveGoalData(replacement->id)) {
            fs::rename(archivedRoot / id, goalsRoot / id, error);
            return false;
        }
        activeId_ = replacement->id;
    }

    goals_.erase(goal);
    archivedGoals_.push_back(archivedGoal);
    if (!SaveIndex() || !SaveArchivedIndex()) {
        return false;
    }
    return true;
}

bool GoalManager::LoadArchivedIndex() {
    archivedGoals_.clear();
    const auto archivedRoot = GetArchivedRoot();
    if (archivedRoot.empty()) {
        return false;
    }

    std::ifstream file(archivedRoot / L"index.txt");
    if (!file) {
        return true;
    }

    std::string line;
    while (std::getline(file, line)) {
        const auto separator = line.find('|');
        if (separator == std::string::npos) {
            continue;
        }
        GoalInfo goal{
            Utf8ToWide(line.substr(0, separator)),
            Utf8ToWide(line.substr(separator + 1))};
        if (!goal.id.empty()) {
            archivedGoals_.push_back(std::move(goal));
        }
    }
    return true;
}

bool GoalManager::SaveArchivedIndex() const {
    const auto archivedRoot = GetArchivedRoot();
    if (archivedRoot.empty()) {
        return false;
    }

    std::error_code error;
    fs::create_directories(archivedRoot, error);
    if (error) {
        return false;
    }

    std::ofstream file(archivedRoot / L"index.txt", std::ios::trunc);
    if (!file) {
        return false;
    }
    for (const GoalInfo& goal : archivedGoals_) {
        file << WideToUtf8(goal.id) << '|' << WideToUtf8(goal.name) << '\n';
    }
    return file.good();
}
