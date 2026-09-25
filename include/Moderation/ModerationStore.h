#pragma once

#include <dpp/dpp.h>

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

enum class CaseType {
    Warn,
    Mute,
    Unmute,
    Kick,
    Ban,
    Tempban,
    Unban
};

struct ModerationCase {
    int64_t mId = 0;
    CaseType mType = CaseType::Warn;
    dpp::snowflake mUserId;
    dpp::snowflake mModeratorId;
    std::string mReason;
    int64_t mCreatedAt = 0;
    int64_t mExpiresAt = 0;
    bool mActive = true;
};

class ModerationStore {
public:
    explicit ModerationStore(std::filesystem::path path);

    bool load(std::string &error);
    const std::filesystem::path &getPath() const;

    int64_t addCase(ModerationCase moderationCase);
    std::vector<ModerationCase> getCases(dpp::snowflake userId) const;
    std::vector<ModerationCase> getActiveWarnings(dpp::snowflake userId) const;
    size_t clearWarnings(dpp::snowflake userId);
    void closeActive(dpp::snowflake userId, CaseType type);
    std::vector<ModerationCase> takeExpiredTempbans(int64_t now);
    size_t getCaseCount() const;

    static std::string toString(CaseType type);
    static std::optional<CaseType> fromString(const std::string &text);

private:
    std::filesystem::path mPath;
    mutable std::mutex mMutex;
    int64_t mNextId = 1;
    std::vector<ModerationCase> mCases;

    void _save() const;
};
