#include "Commands/Moderation/KickCommand.h"

KickCommand::KickCommand(dpp::cluster &bot, ModerationStore &store, ModerationLog &log)
    : ModerationCommand(bot, store, log, dpp::p_kick_members) {
}

std::string KickCommand::getName() const {
    return "kick";
}

dpp::slashcommand KickCommand::getDefinition() const {
    dpp::slashcommand command = makeDefinition("Kick a member from the server");
    command.add_option(dpp::command_option(dpp::co_user, "user", "Member to kick", true));
    command.add_option(dpp::command_option(dpp::co_string, "reason", "Reason, shown in the logs").set_max_length(500));
    return command;
}

dpp::task<void> KickCommand::run(const dpp::slashcommand_t &event) {
    const dpp::snowflake targetId = *readUser(event, "user");
    const std::string reason = readReason(event);
    const std::string name = getUserName(event, targetId);

    const std::string error = co_await checkTarget(event, targetId, true);
    if (!error.empty()) {
        co_await event.co_reply(makeReply(COLOR_ERROR, error, true));
        co_return;
    }

    co_await event.co_thinking(false);

    const std::string notice = "You were kicked from **" + getGuildName(event.command.guild_id) + "**.\nReason: " +
                               reason;
    co_await notify(targetId, notice);

    mBot.set_audit_reason(reason);
    const dpp::confirmation_callback_t result = co_await mBot.co_guild_member_kick(event.command.guild_id, targetId);
    if (result.is_error()) {
        co_await event.co_edit_original_response(makeReply(COLOR_ERROR, "Could not kick **" + name + "**: " +
                                                                        result.get_error().human_readable, false));
        co_return;
    }

    ModerationCase moderationCase = makeCase(event, CaseType::Kick, targetId, reason);
    moderationCase.mId = mStore.addCase(moderationCase);
    mLog.recordCase(moderationCase, name);

    co_await event.co_edit_original_response(makeReply(ModerationLog::getColor(CaseType::Kick),
                                                       "**" + name + "** was kicked. Case #" +
                                                       std::to_string(moderationCase.mId) + "\nReason: " + reason,
                                                       false));
}
