#pragma once

#include "Config.h"
#include "Moderation/ModerationLog.h"
#include "Moderation/ModerationStore.h"

#include <dpp/dpp.h>

#include <chrono>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class AutoModerator {
public:
    AutoModerator(dpp::cluster &bot, const Config &config, ModerationStore &store, ModerationLog &log);

    dpp::task<void> onMessageCreate(dpp::message_create_t event);
    dpp::task<void> onMessageUpdate(dpp::message_update_t event);

private:
    using Clock = std::chrono::steady_clock;

    struct RecentMessage {
        Clock::time_point mTime;
        dpp::snowflake mId;
        dpp::snowflake mChannelId;
        std::string mContent;
    };

    dpp::cluster &mBot;
    const Config &mConfig;
    ModerationStore &mStore;
    ModerationLog &mLog;
    std::mutex mMutex;
    std::unordered_map<dpp::snowflake, std::deque<RecentMessage>> mHistory;

    bool _isModerated(const dpp::message &message) const;
    dpp::task<bool> _handleInvites(const dpp::message &message);
    dpp::task<bool> _isForeignInvite(std::string code);
    std::vector<RecentMessage> _recordAndDetectSpam(const dpp::message &message, std::string &reason);
    dpp::task<void> _punishSpam(const dpp::message &message, std::vector<RecentMessage> messages,
                                std::string reason);
    dpp::task<void> _warn(dpp::snowflake channelId, dpp::snowflake userId, std::string text);
};
