#include "Commands/Moderation/HistoryCommand.h"

#include "Moderation/Duration.h"

HistoryCommand::HistoryCommand(dpp::cluster &bot, ModerationStore &store, ModerationLog &log)
    : ModerationCommand(bot, store, log, dpp::p_moderate_members) {
}

std::string HistoryCommand::getName() const {
    return "history";
}

dpp::slashcommand HistoryCommand::getDefinition() const {
    dpp::slashcommand command = makeDefinition("Show every moderation case of a user");
    command.add_option(dpp::command_option(dpp::co_user, "user", "User to look up", true));
    return command;
}

dpp::task<void> HistoryCommand::run(const dpp::slashcommand_t &event) {
    const dpp::snowflake targetId = *readUser(event, "user");
    const std::string name = getUserName(event, targetId);
    const std::vector<ModerationCase> cases = mStore.getCases(targetId);

    if (cases.empty()) {
        co_await event.co_reply(makeReply(COLOR_INFO, "**" + name + "** has a clean record.", true));
        co_return;
    }

    std::string text = "**" + name + "** has " + std::to_string(cases.size()) + " case" +
                       (cases.size() == 1 ? "" : "s") + ", newest first:\n";
    for (auto it = cases.rbegin(); it != cases.rend(); ++it) {
        std::string line = "\n**#" + std::to_string(it->mId) + " " + ModerationLog::getTitle(it->mType) + "** " +
                           ModerationLog::timestamp(it->mCreatedAt, 'd') + " by " +
                           ModerationLog::mention(it->mModeratorId);
        if (it->mExpiresAt > 0)
            line += " for " + Duration::format(it->mExpiresAt - it->mCreatedAt);
        if (it->mType == CaseType::Warn && !it->mActive)
            line += " (cleared)";
        line += ": " + it->mReason;

        if (text.size() + line.size() > 3900) {
            text += "\n...";
            break;
        }
        text += line;
    }

    co_await event.co_reply(makeReply(COLOR_INFO, text, true));
}
