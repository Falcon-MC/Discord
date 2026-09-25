#include "Commands/Moderation/ModerationCommand.h"

#include <ctime>
#include <variant>

ModerationCommand::ModerationCommand(dpp::cluster &bot, ModerationStore &store, ModerationLog &log,
                                     uint64_t permission)
    : mBot(bot), mStore(store), mLog(log), mPermission(permission) {
}

dpp::task<void> ModerationCommand::execute(dpp::slashcommand_t event) {
    if (event.command.guild_id.empty()) {
        co_await event.co_reply(makeReply(COLOR_ERROR, "This command only works in a server.", true));
        co_return;
    }

    const auto &permissions = event.command.resolved.member_permissions;
    const auto it = permissions.find(event.command.get_issuing_user().id);
    if (it == permissions.end() || !it->second.can(mPermission)) {
        co_await event.co_reply(makeReply(COLOR_ERROR, "You do not have the permission to use this command.", true));
        co_return;
    }

    co_await run(event);
}

dpp::slashcommand ModerationCommand::makeDefinition(const std::string &description) const {
    dpp::slashcommand command(getName(), description, mBot.me.id);
    command.set_default_permissions(mPermission);
    return command;
}

dpp::task<std::string> ModerationCommand::checkTarget(const dpp::slashcommand_t &event, dpp::snowflake targetId,
                                                      bool mustBeMember) {
    const dpp::snowflake invokerId = event.command.get_issuing_user().id;
    if (targetId == invokerId)
        co_return "You cannot do this to yourself.";

    if (targetId == mBot.me.id)
        co_return "You cannot do this to the bot.";

    const dpp::guild *guild = dpp::find_guild(event.command.guild_id);
    if (guild != nullptr && targetId == guild->owner_id)
        co_return "You cannot do this to the server owner.";

    const auto &members = event.command.resolved.members;
    const auto target = members.find(targetId);
    if (target == members.end()) {
        if (mustBeMember)
            co_return "This user is not a member of the server.";
        co_return "";
    }

    const uint8_t targetTop = _getTopPosition(target->second.get_roles());
    const bool invokerIsOwner = guild != nullptr && invokerId == guild->owner_id;
    if (!invokerIsOwner && _getTopPosition(event.command.member.get_roles()) <= targetTop)
        co_return "Your highest role must be above theirs.";

    const dpp::confirmation_callback_t self = co_await mBot.co_guild_get_member(event.command.guild_id, mBot.me.id);
    if (self.is_error())
        co_return "I could not read my own roles, try again.";

    if (_getTopPosition(self.get<dpp::guild_member>().get_roles()) <= targetTop)
        co_return "My highest role must be above theirs.";

    co_return "";
}

dpp::task<void> ModerationCommand::notify(dpp::snowflake userId, std::string text) {
    const dpp::message message(text);
    co_await mBot.co_direct_message_create(userId, message);
}

ModerationCase ModerationCommand::makeCase(const dpp::slashcommand_t &event, CaseType type,
                                           dpp::snowflake targetId, const std::string &reason) const {
    ModerationCase moderationCase;
    moderationCase.mType = type;
    moderationCase.mUserId = targetId;
    moderationCase.mModeratorId = event.command.get_issuing_user().id;
    moderationCase.mReason = reason;
    moderationCase.mCreatedAt = static_cast<int64_t>(std::time(nullptr));
    return moderationCase;
}

dpp::message ModerationCommand::makeReply(uint32_t color, const std::string &text, bool ephemeral) {
    dpp::message message;
    message.add_embed(dpp::embed().set_color(color).set_description(text));
    if (ephemeral)
        message.set_flags(dpp::m_ephemeral);
    return message;
}

std::string ModerationCommand::readString(const dpp::slashcommand_t &event, const std::string &name) {
    const dpp::command_value value = event.get_parameter(name);
    if (!std::holds_alternative<std::string>(value))
        return {};

    return std::get<std::string>(value);
}

std::optional<int64_t> ModerationCommand::readInteger(const dpp::slashcommand_t &event, const std::string &name) {
    const dpp::command_value value = event.get_parameter(name);
    if (!std::holds_alternative<int64_t>(value))
        return std::nullopt;

    return std::get<int64_t>(value);
}

std::optional<dpp::snowflake> ModerationCommand::readUser(const dpp::slashcommand_t &event,
                                                          const std::string &name) {
    const dpp::command_value value = event.get_parameter(name);
    if (!std::holds_alternative<dpp::snowflake>(value))
        return std::nullopt;

    return std::get<dpp::snowflake>(value);
}

std::string ModerationCommand::readReason(const dpp::slashcommand_t &event) {
    const std::string reason = readString(event, "reason");
    return reason.empty() ? "No reason given" : reason;
}

std::string ModerationCommand::getUserName(const dpp::slashcommand_t &event, dpp::snowflake userId) {
    const auto &users = event.command.resolved.users;
    const auto it = users.find(userId);
    if (it == users.end())
        return userId.str();

    return it->second.username;
}

std::string ModerationCommand::getGuildName(dpp::snowflake guildId) {
    const dpp::guild *guild = dpp::find_guild(guildId);
    return guild == nullptr ? "the server" : guild->name;
}

uint8_t ModerationCommand::_getTopPosition(const std::vector<dpp::snowflake> &roles) {
    uint8_t top = 0;
    for (const dpp::snowflake &roleId : roles) {
        const dpp::role *role = dpp::find_role(roleId);
        if (role != nullptr && role->position > top)
            top = role->position;
    }
    return top;
}
