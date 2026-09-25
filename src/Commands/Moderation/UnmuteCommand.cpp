#include "Commands/Moderation/UnmuteCommand.h"

UnmuteCommand::UnmuteCommand(dpp::cluster &bot, ModerationStore &store, ModerationLog &log)
    : ModerationCommand(bot, store, log, dpp::p_moderate_members) {
}

std::string UnmuteCommand::getName() const {
    return "unmute";
}

dpp::slashcommand UnmuteCommand::getDefinition() const {
    dpp::slashcommand command = makeDefinition("Remove the timeout of a member");
    command.add_option(dpp::command_option(dpp::co_user, "user", "Member to unmute", true));
    command.add_option(dpp::command_option(dpp::co_string, "reason", "Reason, shown in the logs").set_max_length(500));
    return command;
}

dpp::task<void> UnmuteCommand::run(const dpp::slashcommand_t &event) {
    const dpp::snowflake targetId = *readUser(event, "user");
    const std::string reason = readReason(event);
    const std::string name = getUserName(event, targetId);

    const std::string error = co_await checkTarget(event, targetId, true);
    if (!error.empty()) {
        co_await event.co_reply(makeReply(COLOR_ERROR, error, true));
        co_return;
    }

    co_await event.co_thinking(false);

    mBot.set_audit_reason(reason);
    const dpp::confirmation_callback_t result = co_await mBot.co_guild_member_timeout(event.command.guild_id,
                                                                                      targetId, 0);
    if (result.is_error()) {
        co_await event.co_edit_original_response(makeReply(COLOR_ERROR, "Could not unmute **" + name + "**: " +
                                                                        result.get_error().human_readable, false));
        co_return;
    }

    mStore.closeActive(targetId, CaseType::Mute);
    ModerationCase moderationCase = makeCase(event, CaseType::Unmute, targetId, reason);
    moderationCase.mId = mStore.addCase(moderationCase);
    mLog.recordCase(moderationCase, name);

    co_await event.co_edit_original_response(makeReply(ModerationLog::getColor(CaseType::Unmute),
                                                       "**" + name + "** was unmuted. Case #" +
                                                       std::to_string(moderationCase.mId), false));
}
