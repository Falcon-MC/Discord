#include "Moderation/ModerationLog.h"

#include "Moderation/Duration.h"

#include <ctime>

ModerationLog::ModerationLog(dpp::cluster &bot, const Config &config)
    : mBot(bot), mConfig(config) {
}

void ModerationLog::recordCase(const ModerationCase &moderationCase, const std::string &userName) const {
    const std::string title = "Case #" + std::to_string(moderationCase.mId) + " - " +
                              getTitle(moderationCase.mType);
    mBot.log(dpp::ll_info, "Moderation: " + title + " for " + userName + " (" + moderationCase.mUserId.str() +
                           ") by " + moderationCase.mModeratorId.str() + ": " + moderationCase.mReason);

    dpp::embed embed;
    embed.set_color(getColor(moderationCase.mType));
    embed.set_title(title);
    embed.add_field("Member", mention(moderationCase.mUserId) + " (" + userName + ")", true);
    embed.add_field("Moderator", mention(moderationCase.mModeratorId), true);
    if (moderationCase.mExpiresAt > 0) {
        embed.add_field("Duration", Duration::format(moderationCase.mExpiresAt - moderationCase.mCreatedAt) +
                                    ", ends " + timestamp(moderationCase.mExpiresAt, 'R'), true);
    }
    embed.add_field("Reason", moderationCase.mReason, false);
    embed.set_timestamp(static_cast<time_t>(moderationCase.mCreatedAt));
    _send(embed);
}

void ModerationLog::recordEvent(const std::string &title, dpp::snowflake userId, const std::string &userName,
                                dpp::snowflake channelId, const std::string &details) const {
    mBot.log(dpp::ll_info, "Moderation: " + title + " for " + userName + " (" + userId.str() + ") in channel " +
                           channelId.str());

    std::string text = details;
    if (text.size() > 1000)
        text = text.substr(0, 1000) + "...";
    if (text.empty())
        text = "*no text*";

    dpp::embed embed;
    embed.set_color(0xEF4444);
    embed.set_title(title);
    embed.add_field("Member", mention(userId) + " (" + userName + ")", true);
    embed.add_field("Channel", "<#" + channelId.str() + ">", true);
    embed.add_field("Details", text, false);
    embed.set_timestamp(std::time(nullptr));
    _send(embed);
}

uint32_t ModerationLog::getColor(CaseType type) {
    switch (type) {
        case CaseType::Warn:
            return 0xEAB308;
        case CaseType::Mute:
            return 0x8B5CF6;
        case CaseType::Kick:
            return 0xF59E0B;
        case CaseType::Ban:
            return 0xEF4444;
        case CaseType::Tempban:
            return 0xF97316;
        case CaseType::Unmute:
        case CaseType::Unban:
            return 0x10B981;
    }
    return 0xE67E22;
}

std::string ModerationLog::getTitle(CaseType type) {
    switch (type) {
        case CaseType::Warn:
            return "Warn";
        case CaseType::Mute:
            return "Mute";
        case CaseType::Unmute:
            return "Unmute";
        case CaseType::Kick:
            return "Kick";
        case CaseType::Ban:
            return "Ban";
        case CaseType::Tempban:
            return "Tempban";
        case CaseType::Unban:
            return "Unban";
    }
    return "Case";
}

std::string ModerationLog::mention(dpp::snowflake id) {
    return "<@" + id.str() + ">";
}

std::string ModerationLog::timestamp(int64_t epoch, char style) {
    return "<t:" + std::to_string(epoch) + ":" + std::string(1, style) + ">";
}

void ModerationLog::_send(const dpp::embed &embed) const {
    if (mConfig.mModLogChannelId.empty())
        return;

    mBot.message_create(dpp::message(mConfig.mModLogChannelId, embed));
}
