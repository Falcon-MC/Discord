#include "Commands/Moderation/BanCommand.h"

BanCommand::BanCommand(dpp::cluster &bot, ModerationStore &store, ModerationLog &log)
    : ModerationCommand(bot, store, log, dpp::p_ban_members) {
}

std::string BanCommand::getName() const {
    return "ban";
}

dpp::slashcommand BanCommand::getDefinition() const {
    dpp::slashcommand command = makeDefinition("Ban a user from the server");
    command.add_option(dpp::command_option(dpp::co_user, "user", "User to ban", true));
    command.add_option(dpp::command_option(dpp::co_string, "reason", "Reason, shown in the logs").set_max_length(500));
    command.add_option(dpp::command_option(dpp::co_integer, "delete_days", "Delete their messages from the last "
                                                                           "days (0 to 7)")
                           .set_min_value(0)
                           .set_max_value(7));
    return command;
}

dpp::task<void> BanCommand::run(const dpp::slashcommand_t &event) {
    const dpp::snowflake targetId = *readUser(event, "user");
    const std::string reason = readReason(event);
    const int64_t deleteDays = readInteger(event, "delete_days").value_or(0);
    const std::string name = getUserName(event, targetId);

    const std::string error = co_await checkTarget(event, targetId, false);
    if (!error.empty()) {
        co_await event.co_reply(makeReply(COLOR_ERROR, error, true));
        co_return;
    }

    co_await event.co_thinking(false);

    const std::string notice = "You were banned from **" + getGuildName(event.command.guild_id) + "**.\nReason: " +
                               reason;
    co_await notify(targetId, notice);

    mBot.set_audit_reason(reason);
    const dpp::confirmation_callback_t result = co_await mBot.co_guild_ban_add(
        event.command.guild_id, targetId, static_cast<uint32_t>(deleteDays * 24 * 60 * 60));
    if (result.is_error()) {
        co_await event.co_edit_original_response(makeReply(COLOR_ERROR, "Could not ban **" + name + "**: " +
                                                                        result.get_error().human_readable, false));
        co_return;
    }

    mStore.closeActive(targetId, CaseType::Tempban);
    ModerationCase moderationCase = makeCase(event, CaseType::Ban, targetId, reason);
    moderationCase.mId = mStore.addCase(moderationCase);
    mLog.recordCase(moderationCase, name);

    co_await event.co_edit_original_response(makeReply(ModerationLog::getColor(CaseType::Ban),
                                                       "**" + name + "** was banned. Case #" +
                                                       std::to_string(moderationCase.mId) + "\nReason: " + reason,
                                                       false));
}
