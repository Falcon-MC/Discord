#include "Commands/Moderation/WarningsCommand.h"

WarningsCommand::WarningsCommand(dpp::cluster &bot, ModerationStore &store, ModerationLog &log)
    : ModerationCommand(bot, store, log, dpp::p_moderate_members) {
}

std::string WarningsCommand::getName() const {
    return "warnings";
}

dpp::slashcommand WarningsCommand::getDefinition() const {
    dpp::slashcommand command = makeDefinition("List the active warnings of a user");
    command.add_option(dpp::command_option(dpp::co_user, "user", "User to look up", true));
    return command;
}

dpp::task<void> WarningsCommand::run(const dpp::slashcommand_t &event) {
    const dpp::snowflake targetId = *readUser(event, "user");
    const std::string name = getUserName(event, targetId);
    const std::vector<ModerationCase> warnings = mStore.getActiveWarnings(targetId);

    if (warnings.empty()) {
        co_await event.co_reply(makeReply(COLOR_INFO, "**" + name + "** has no active warnings.", true));
        co_return;
    }

    std::string text = "**" + name + "** has " + std::to_string(warnings.size()) + " active warning" +
                       (warnings.size() == 1 ? "" : "s") + ":\n";
    for (auto it = warnings.rbegin(); it != warnings.rend(); ++it) {
        const std::string line = "\n**#" + std::to_string(it->mId) + "** " +
                                 ModerationLog::timestamp(it->mCreatedAt, 'd') + " by " +
                                 ModerationLog::mention(it->mModeratorId) + ": " + it->mReason;
        if (text.size() + line.size() > 3900) {
            text += "\n...";
            break;
        }
        text += line;
    }

    co_await event.co_reply(makeReply(ModerationLog::getColor(CaseType::Warn), text, true));
}
