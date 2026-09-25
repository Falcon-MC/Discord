#include "Diagnostics/GuildDiagnostics.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {
    const std::vector<std::pair<uint64_t, std::string>> REQUIRED_PERMISSIONS = {
        {dpp::p_view_channel, "View Channels"},
        {dpp::p_send_messages, "Send Messages"},
        {dpp::p_embed_links, "Embed Links"},
        {dpp::p_manage_roles, "Manage Roles"},
        {dpp::p_manage_messages, "Manage Messages"},
        {dpp::p_read_message_history, "Read Message History"},
        {dpp::p_moderate_members, "Moderate Members"},
        {dpp::p_kick_members, "Kick Members"},
        {dpp::p_ban_members, "Ban Members"}
    };
}

GuildDiagnostics::GuildDiagnostics(dpp::cluster &bot, const Config &config)
    : mBot(bot), mConfig(config) {
}

void GuildDiagnostics::logConfiguration() const {
    mBot.log(dpp::ll_info, "Configuration loaded");
    mBot.log(dpp::ll_info, "  Guild: " + mConfig.mGuildId.str());
    mBot.log(dpp::ll_info, "  Contributor role: " + mConfig.mContributorRoleId.str());
    mBot.log(dpp::ll_info, "  Moderation log channel: " +
                           (mConfig.mModLogChannelId.empty() ? std::string("disabled") :
                                                               mConfig.mModLogChannelId.str()));
    mBot.log(dpp::ll_info, "  GitHub organization: " + mConfig.mGitHubOrganization);
    mBot.log(dpp::ll_info, "  Data directory: " + mConfig.mDataDirectory);
    mBot.log(dpp::ll_info, "  Intents: guilds, guild messages, message content");
}

dpp::task<void> GuildDiagnostics::onGuildCreate(dpp::guild_create_t event) {
    const dpp::guild &guild = event.created;
    if (guild.id != mConfig.mGuildId) {
        mBot.log(dpp::ll_warning, "Ignoring guild " + guild.name + " (" + guild.id.str() +
                                  "), it is not DISCORD_GUILD_ID");
        co_return;
    }

    mBot.log(dpp::ll_info, "Found guild " + guild.name + " (" + guild.id.str() + "), " +
                           std::to_string(guild.channels.size()) + " channels, " +
                           std::to_string(guild.roles.size()) + " roles");
    _checkRole(guild);
    _checkModLogChannel(guild);
    co_await _checkBotPermissions(guild);
}

void GuildDiagnostics::_checkRole(const dpp::guild &guild) const {
    const dpp::role *role = dpp::find_role(mConfig.mContributorRoleId);
    if (role == nullptr || role->guild_id != guild.id) {
        mBot.log(dpp::ll_error, "Contributor role " + mConfig.mContributorRoleId.str() + " not found in " +
                                guild.name + ", /contributor will fail");
        return;
    }

    mBot.log(dpp::ll_info, "Found role @" + role->name + " (" + role->id.str() + ")");
}

void GuildDiagnostics::_checkModLogChannel(const dpp::guild &guild) const {
    if (mConfig.mModLogChannelId.empty()) {
        mBot.log(dpp::ll_info, "No moderation log channel set, moderation actions are only logged here");
        return;
    }

    const dpp::channel *channel = dpp::find_channel(mConfig.mModLogChannelId);
    if (channel == nullptr || channel->guild_id != guild.id) {
        mBot.log(dpp::ll_error, "Moderation log channel " + mConfig.mModLogChannelId.str() + " not found in " +
                                guild.name);
        return;
    }

    mBot.log(dpp::ll_info, "Found channel #" + channel->name + " (" + channel->id.str() + ") for moderation logs");
}

dpp::task<void> GuildDiagnostics::_checkBotPermissions(dpp::guild guild) {
    const dpp::confirmation_callback_t result = co_await mBot.co_guild_get_member(guild.id, mBot.me.id);
    if (result.is_error()) {
        mBot.log(dpp::ll_error, "Could not read the bot's own member: " + result.get_error().human_readable);
        co_return;
    }

    const dpp::guild_member member = result.get<dpp::guild_member>();
    const dpp::permission permissions = guild.base_permissions(member);

    std::string missing;
    for (const auto &[flag, name] : REQUIRED_PERMISSIONS) {
        if (permissions.can(flag))
            continue;

        if (!missing.empty())
            missing += ", ";
        missing += name;
    }

    if (missing.empty()) {
        mBot.log(dpp::ll_info, "Bot permissions OK");
    } else {
        mBot.log(dpp::ll_error, "Bot is missing permissions: " + missing);
    }

    uint8_t highestPosition = 0;
    for (const dpp::snowflake &roleId : member.get_roles()) {
        const dpp::role *role = dpp::find_role(roleId);
        if (role != nullptr && role->position > highestPosition)
            highestPosition = role->position;
    }

    const dpp::role *contributor = dpp::find_role(mConfig.mContributorRoleId);
    if (contributor != nullptr && contributor->position >= highestPosition) {
        mBot.log(dpp::ll_error, "The bot's highest role must be above @" + contributor->name +
                                " in the role list, otherwise it cannot give it");
    }
}
