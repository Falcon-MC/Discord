#include "Moderation/AutoModerator.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <regex>
#include <unordered_set>

namespace {
    constexpr auto FLOOD_WINDOW = std::chrono::seconds(6);
    constexpr size_t FLOOD_LIMIT = 6;
    constexpr auto DUPLICATE_WINDOW = std::chrono::seconds(30);
    constexpr size_t DUPLICATE_LIMIT = 3;
    constexpr size_t MENTION_LIMIT = 5;
    constexpr time_t TIMEOUT_SECONDS = 10 * 60;
    constexpr uint64_t WARNING_LIFETIME = 8;
    constexpr uint32_t COLOR_MODERATION = 0xEF4444;

    const std::regex INVITE_PATTERN(R"((?:discord\.gg|discord(?:app)?\.com/invite)/([A-Za-z0-9-]+))",
                                    std::regex::icase);

    std::string normalize(const std::string &content) {
        std::string result;
        result.reserve(content.size());
        for (const char character : content) {
            result += static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        }

        const size_t first = result.find_first_not_of(" \t\r\n");
        if (first == std::string::npos)
            return {};

        const size_t last = result.find_last_not_of(" \t\r\n");
        return result.substr(first, last - first + 1);
    }

    std::vector<std::string> findInviteCodes(const std::string &content) {
        std::vector<std::string> codes;
        std::unordered_set<std::string> seen;
        for (auto it = std::sregex_iterator(content.begin(), content.end(), INVITE_PATTERN);
             it != std::sregex_iterator(); ++it) {
            const std::string code = (*it)[1].str();
            if (seen.insert(code).second)
                codes.push_back(code);
        }
        return codes;
    }
}

AutoModerator::AutoModerator(dpp::cluster &bot, const Config &config)
    : mBot(bot), mConfig(config) {
}

dpp::task<void> AutoModerator::onMessageCreate(dpp::message_create_t event) {
    if (!_isModerated(event.msg))
        co_return;

    const bool removed = co_await _handleInvites(event.msg);
    if (removed)
        co_return;

    std::string reason;
    std::vector<RecentMessage> spam = _recordAndDetectSpam(event.msg, reason);
    if (spam.empty())
        co_return;

    co_await _punishSpam(event.msg, spam, reason);
}

dpp::task<void> AutoModerator::onMessageUpdate(dpp::message_update_t event) {
    if (!_isModerated(event.msg))
        co_return;

    co_await _handleInvites(event.msg);
}

bool AutoModerator::_isModerated(const dpp::message &message) const {
    if (message.guild_id != mConfig.mGuildId)
        return false;

    if (message.author.id.empty() || message.author.is_bot() || !message.webhook_id.empty())
        return false;

    const dpp::guild *guild = dpp::find_guild(message.guild_id);
    if (guild == nullptr)
        return true;

    const dpp::permission permissions = guild->base_permissions(message.member);
    return !permissions.can(dpp::p_manage_messages);
}

dpp::task<bool> AutoModerator::_handleInvites(const dpp::message &message) {
    const std::vector<std::string> codes = findInviteCodes(message.content);
    for (const std::string &code : codes) {
        const bool foreign = co_await _isForeignInvite(code);
        if (!foreign)
            continue;

        co_await mBot.co_message_delete(message.id, message.channel_id);
        _log(message, "Invite to another Discord server");

        const std::string text = "Invites to other Discord servers are not allowed here.";
        co_await _warn(message.channel_id, message.author.id, text);
        co_return true;
    }
    co_return false;
}

dpp::task<bool> AutoModerator::_isForeignInvite(std::string code) {
    const dpp::confirmation_callback_t result = co_await mBot.co_invite_get(code);
    if (result.is_error())
        co_return true;

    co_return result.get<dpp::invite>().guild_id != mConfig.mGuildId;
}

std::vector<AutoModerator::RecentMessage> AutoModerator::_recordAndDetectSpam(const dpp::message &message,
                                                                              std::string &reason) {
    RecentMessage current;
    current.mTime = Clock::now();
    current.mId = message.id;
    current.mChannelId = message.channel_id;
    current.mContent = normalize(message.content);

    if (message.mentions.size() + message.mention_roles.size() >= MENTION_LIMIT) {
        reason = "Mass mentions";
        return {current};
    }

    std::lock_guard<std::mutex> lock(mMutex);
    std::deque<RecentMessage> &history = mHistory[message.author.id];
    history.push_back(current);
    while (!history.empty() && current.mTime - history.front().mTime > DUPLICATE_WINDOW) {
        history.pop_front();
    }

    std::vector<RecentMessage> flood;
    std::vector<RecentMessage> duplicates;
    for (const RecentMessage &recent : history) {
        if (current.mTime - recent.mTime <= FLOOD_WINDOW)
            flood.push_back(recent);
        if (!current.mContent.empty() && recent.mContent == current.mContent)
            duplicates.push_back(recent);
    }

    std::vector<RecentMessage> spam;
    if (flood.size() >= FLOOD_LIMIT) {
        reason = "Message flood";
        spam = flood;
    } else if (duplicates.size() >= DUPLICATE_LIMIT) {
        reason = "Repeated messages";
        spam = duplicates;
    }

    if (!spam.empty())
        mHistory.erase(message.author.id);

    return spam;
}

dpp::task<void> AutoModerator::_punishSpam(const dpp::message &message, std::vector<RecentMessage> messages,
                                           std::string reason) {
    for (const RecentMessage &recent : messages) {
        co_await mBot.co_message_delete(recent.mId, recent.mChannelId);
    }

    const time_t until = std::time(nullptr) + TIMEOUT_SECONDS;
    const dpp::confirmation_callback_t result = co_await mBot.co_guild_member_timeout(message.guild_id,
                                                                                      message.author.id, until);
    if (result.is_error()) {
        mBot.log(dpp::ll_error, "Could not time out " + message.author.username + ": " +
                                result.get_error().human_readable);
    }

    _log(message, reason + ", timed out for 10 minutes");

    const std::string text = "Slow down. You are timed out for 10 minutes for spamming.";
    co_await _warn(message.channel_id, message.author.id, text);
}

dpp::task<void> AutoModerator::_warn(dpp::snowflake channelId, dpp::snowflake userId, std::string text) {
    dpp::message warning(channelId, "<@" + std::to_string(static_cast<uint64_t>(userId)) + "> " + text);
    warning.allowed_mentions.users.push_back(userId);

    const dpp::confirmation_callback_t sent = co_await mBot.co_message_create(warning);
    if (sent.is_error())
        co_return;

    const dpp::message posted = sent.get<dpp::message>();
    co_await mBot.co_sleep(WARNING_LIFETIME);
    co_await mBot.co_message_delete(posted.id, posted.channel_id);
}

void AutoModerator::_log(const dpp::message &message, const std::string &reason) {
    if (mConfig.mModLogChannelId.empty())
        return;

    std::string content = message.content;
    if (content.size() > 1000)
        content = content.substr(0, 1000) + "...";
    if (content.empty())
        content = "*no text*";

    dpp::embed embed;
    embed.set_color(COLOR_MODERATION);
    embed.set_title(reason);
    embed.add_field("Member", "<@" + std::to_string(static_cast<uint64_t>(message.author.id)) + "> (" +
                                  message.author.username + ")", true);
    embed.add_field("Channel", "<#" + std::to_string(static_cast<uint64_t>(message.channel_id)) + ">", true);
    embed.add_field("Message", content, false);
    embed.set_timestamp(std::time(nullptr));

    mBot.message_create(dpp::message(mConfig.mModLogChannelId, embed));
}
