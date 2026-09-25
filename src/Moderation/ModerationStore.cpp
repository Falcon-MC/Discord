#include "Moderation/ModerationStore.h"

#include <fstream>
#include <sstream>
#include <utility>

namespace {
    dpp::json toJson(const ModerationCase &moderationCase) {
        dpp::json entry = dpp::json::object();
        entry["id"] = moderationCase.mId;
        entry["type"] = ModerationStore::toString(moderationCase.mType);
        entry["user"] = moderationCase.mUserId.str();
        entry["moderator"] = moderationCase.mModeratorId.str();
        entry["reason"] = moderationCase.mReason;
        entry["createdAt"] = moderationCase.mCreatedAt;
        entry["expiresAt"] = moderationCase.mExpiresAt;
        entry["active"] = moderationCase.mActive;
        return entry;
    }

    std::optional<ModerationCase> fromJson(const dpp::json &entry) {
        if (!entry.is_object())
            return std::nullopt;

        const std::optional<CaseType> type = ModerationStore::fromString(entry.value("type", ""));
        if (!type.has_value())
            return std::nullopt;

        ModerationCase moderationCase;
        moderationCase.mId = entry.value("id", static_cast<int64_t>(0));
        moderationCase.mType = *type;
        moderationCase.mUserId = dpp::snowflake(entry.value("user", "0"));
        moderationCase.mModeratorId = dpp::snowflake(entry.value("moderator", "0"));
        moderationCase.mReason = entry.value("reason", "");
        moderationCase.mCreatedAt = entry.value("createdAt", static_cast<int64_t>(0));
        moderationCase.mExpiresAt = entry.value("expiresAt", static_cast<int64_t>(0));
        moderationCase.mActive = entry.value("active", false);
        return moderationCase;
    }
}

ModerationStore::ModerationStore(std::filesystem::path path)
    : mPath(std::move(path)) {
}

bool ModerationStore::load(std::string &error) {
    std::lock_guard<std::mutex> lock(mMutex);
    std::error_code code;
    if (mPath.has_parent_path())
        std::filesystem::create_directories(mPath.parent_path(), code);

    if (!std::filesystem::exists(mPath)) {
        _save();
        return true;
    }

    std::ifstream input(mPath);
    std::stringstream buffer;
    buffer << input.rdbuf();

    const dpp::json root = dpp::json::parse(buffer.str(), nullptr, false);
    if (root.is_discarded() || !root.is_object() || !root.contains("cases") || !root["cases"].is_array()) {
        error = mPath.string() + " is not a valid moderation file";
        return false;
    }

    mCases.clear();
    for (const dpp::json &entry : root["cases"]) {
        const std::optional<ModerationCase> moderationCase = fromJson(entry);
        if (moderationCase.has_value())
            mCases.push_back(*moderationCase);
    }

    mNextId = root.value("nextCase", static_cast<int64_t>(1));
    for (const ModerationCase &moderationCase : mCases) {
        if (moderationCase.mId >= mNextId)
            mNextId = moderationCase.mId + 1;
    }
    return true;
}

const std::filesystem::path &ModerationStore::getPath() const {
    return mPath;
}

int64_t ModerationStore::addCase(ModerationCase moderationCase) {
    std::lock_guard<std::mutex> lock(mMutex);
    moderationCase.mId = mNextId++;
    mCases.push_back(moderationCase);
    _save();
    return moderationCase.mId;
}

std::vector<ModerationCase> ModerationStore::getCases(dpp::snowflake userId) const {
    std::lock_guard<std::mutex> lock(mMutex);
    std::vector<ModerationCase> result;
    for (const ModerationCase &moderationCase : mCases) {
        if (moderationCase.mUserId == userId)
            result.push_back(moderationCase);
    }
    return result;
}

std::vector<ModerationCase> ModerationStore::getActiveWarnings(dpp::snowflake userId) const {
    std::lock_guard<std::mutex> lock(mMutex);
    std::vector<ModerationCase> result;
    for (const ModerationCase &moderationCase : mCases) {
        if (moderationCase.mUserId == userId && moderationCase.mType == CaseType::Warn && moderationCase.mActive)
            result.push_back(moderationCase);
    }
    return result;
}

size_t ModerationStore::clearWarnings(dpp::snowflake userId) {
    std::lock_guard<std::mutex> lock(mMutex);
    size_t cleared = 0;
    for (ModerationCase &moderationCase : mCases) {
        if (moderationCase.mUserId == userId && moderationCase.mType == CaseType::Warn && moderationCase.mActive) {
            moderationCase.mActive = false;
            ++cleared;
        }
    }

    if (cleared > 0)
        _save();

    return cleared;
}

void ModerationStore::closeActive(dpp::snowflake userId, CaseType type) {
    std::lock_guard<std::mutex> lock(mMutex);
    bool changed = false;
    for (ModerationCase &moderationCase : mCases) {
        if (moderationCase.mUserId == userId && moderationCase.mType == type && moderationCase.mActive) {
            moderationCase.mActive = false;
            changed = true;
        }
    }

    if (changed)
        _save();
}

std::vector<ModerationCase> ModerationStore::takeExpiredTempbans(int64_t now) {
    std::lock_guard<std::mutex> lock(mMutex);
    std::vector<ModerationCase> expired;
    for (ModerationCase &moderationCase : mCases) {
        if (moderationCase.mType != CaseType::Tempban || !moderationCase.mActive)
            continue;

        if (moderationCase.mExpiresAt > now)
            continue;

        moderationCase.mActive = false;
        expired.push_back(moderationCase);
    }

    if (!expired.empty())
        _save();

    return expired;
}

size_t ModerationStore::getCaseCount() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mCases.size();
}

std::string ModerationStore::toString(CaseType type) {
    switch (type) {
        case CaseType::Warn:
            return "warn";
        case CaseType::Mute:
            return "mute";
        case CaseType::Unmute:
            return "unmute";
        case CaseType::Kick:
            return "kick";
        case CaseType::Ban:
            return "ban";
        case CaseType::Tempban:
            return "tempban";
        case CaseType::Unban:
            return "unban";
    }
    return "warn";
}

std::optional<CaseType> ModerationStore::fromString(const std::string &text) {
    const std::vector<CaseType> types = {
        CaseType::Warn,
        CaseType::Mute,
        CaseType::Unmute,
        CaseType::Kick,
        CaseType::Ban,
        CaseType::Tempban,
        CaseType::Unban
    };
    for (const CaseType type : types) {
        if (toString(type) == text)
            return type;
    }
    return std::nullopt;
}

void ModerationStore::_save() const {
    dpp::json root = dpp::json::object();
    root["nextCase"] = mNextId;
    root["cases"] = dpp::json::array();
    for (const ModerationCase &moderationCase : mCases) {
        root["cases"].push_back(toJson(moderationCase));
    }

    const std::filesystem::path temporary = mPath.string() + ".tmp";
    {
        std::ofstream output(temporary, std::ios::trunc);
        output << root.dump(2);
    }

    std::error_code code;
    std::filesystem::rename(temporary, mPath, code);
}
