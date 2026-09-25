#include "Commands/Moderation/ClearWarningsCommand.h"

ClearWarningsCommand::ClearWarningsCommand(dpp::cluster &bot, ModerationStore &store, ModerationLog &log)
    : ModerationCommand(bot, store, log, dpp::p_administrator) {
}

std::string ClearWarningsCommand::getName() const {
    return "clearwarnings";
}

dpp::slashcommand ClearWarningsCommand::getDefinition() const {
    dpp::slashcommand command = makeDefinition("Clear all active warnings of a user");
    command.add_option(dpp::command_option(dpp::co_user, "user", "User whose warnings are cleared", true));
    return command;
}

dpp::task<void> ClearWarningsCommand::run(const dpp::slashcommand_t &event) {
    const dpp::snowflake targetId = *readUser(event, "user");
    const std::string name = getUserName(event, targetId);
    const size_t cleared = mStore.clearWarnings(targetId);

    if (cleared == 0) {
        co_await event.co_reply(makeReply(COLOR_INFO, "**" + name + "** has no active warnings.", true));
        co_return;
    }

    mLog.recordEvent("Warnings cleared", targetId, name, event.command.channel_id,
                     std::to_string(cleared) + " warnings cleared by " +
                     event.command.get_issuing_user().username);

    co_await event.co_reply(makeReply(ModerationLog::getColor(CaseType::Unmute),
                                      "Cleared " + std::to_string(cleared) + " warning" +
                                      (cleared == 1 ? "" : "s") + " of **" + name + "**.", false));
}
