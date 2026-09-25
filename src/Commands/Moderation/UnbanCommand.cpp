#include "Commands/Moderation/UnbanCommand.h"

UnbanCommand::UnbanCommand(dpp::cluster &bot, ModerationStore &store, ModerationLog &log)
    : ModerationCommand(bot, store, log, dpp::p_ban_members) {
}

std::string UnbanCommand::getName() const {
    return "unban";
}

dpp::slashcommand UnbanCommand::getDefinition() const {
    dpp::slashcommand command = makeDefinition("Unban a user");
    command.add_option(dpp::command_option(dpp::co_user, "user", "User to unban, or their ID", true));
    command.add_option(dpp::command_option(dpp::co_string, "reason", "Reason, shown in the logs").set_max_length(500));
    return command;
}

dpp::task<void> UnbanCommand::run(const dpp::slashcommand_t &event) {
    const dpp::snowflake targetId = *readUser(event, "user");
    const std::string reason = readReason(event);
    const std::string name = getUserName(event, targetId);

    co_await event.co_thinking(false);

    mBot.set_audit_reason(reason);
    const dpp::confirmation_callback_t result = co_await mBot.co_guild_ban_delete(event.command.guild_id, targetId);
    if (result.is_error()) {
        co_await event.co_edit_original_response(makeReply(COLOR_ERROR, "Could not unban **" + name + "**: " +
                                                                        result.get_error().human_readable, false));
        co_return;
    }

    mStore.closeActive(targetId, CaseType::Tempban);
    ModerationCase moderationCase = makeCase(event, CaseType::Unban, targetId, reason);
    moderationCase.mId = mStore.addCase(moderationCase);
    mLog.recordCase(moderationCase, name);

    co_await event.co_edit_original_response(makeReply(ModerationLog::getColor(CaseType::Unban),
                                                       "**" + name + "** was unbanned. Case #" +
                                                       std::to_string(moderationCase.mId), false));
}
