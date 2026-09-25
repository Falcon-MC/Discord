#include "Commands/Moderation/WarnCommand.h"

WarnCommand::WarnCommand(dpp::cluster &bot, ModerationStore &store, ModerationLog &log)
    : ModerationCommand(bot, store, log, dpp::p_moderate_members) {
}

std::string WarnCommand::getName() const {
    return "warn";
}

dpp::slashcommand WarnCommand::getDefinition() const {
    dpp::slashcommand command = makeDefinition("Warn a member");
    command.add_option(dpp::command_option(dpp::co_user, "user", "Member to warn", true));
    command.add_option(dpp::command_option(dpp::co_string, "reason", "Reason, sent to the member", true)
                           .set_max_length(500));
    return command;
}

dpp::task<void> WarnCommand::run(const dpp::slashcommand_t &event) {
    const dpp::snowflake targetId = *readUser(event, "user");
    const std::string reason = readReason(event);
    const std::string name = getUserName(event, targetId);

    const std::string error = co_await checkTarget(event, targetId, true);
    if (!error.empty()) {
        co_await event.co_reply(makeReply(COLOR_ERROR, error, true));
        co_return;
    }

    ModerationCase moderationCase = makeCase(event, CaseType::Warn, targetId, reason);
    moderationCase.mId = mStore.addCase(moderationCase);
    mLog.recordCase(moderationCase, name);

    const size_t count = mStore.getActiveWarnings(targetId).size();
    co_await event.co_reply(makeReply(ModerationLog::getColor(CaseType::Warn),
                                      "**" + name + "** was warned, " + std::to_string(count) +
                                      (count == 1 ? " warning" : " warnings") + " in total. Case #" +
                                      std::to_string(moderationCase.mId) + "\nReason: " + reason, false));

    const std::string notice = "You were warned in **" + getGuildName(event.command.guild_id) + "**.\nReason: " +
                               reason;
    co_await notify(targetId, notice);
}
