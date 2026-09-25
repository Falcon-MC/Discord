#include "Commands/Moderation/TempbanCommand.h"

#include "Moderation/Duration.h"

namespace {
    constexpr int64_t MAX_TEMPBAN_SECONDS = 365LL * 24 * 60 * 60;
}

TempbanCommand::TempbanCommand(dpp::cluster &bot, ModerationStore &store, ModerationLog &log)
    : ModerationCommand(bot, store, log, dpp::p_ban_members) {
}

std::string TempbanCommand::getName() const {
    return "tempban";
}

dpp::slashcommand TempbanCommand::getDefinition() const {
    dpp::slashcommand command = makeDefinition("Ban a user for a limited time");
    command.add_option(dpp::command_option(dpp::co_user, "user", "User to ban", true));
    command.add_option(dpp::command_option(dpp::co_string, "duration", "Duration, e.g. 30m, 12h, 7d, 1w2d", true)
                           .set_max_length(20));
    command.add_option(dpp::command_option(dpp::co_string, "reason", "Reason, shown in the logs").set_max_length(500));
    return command;
}

dpp::task<void> TempbanCommand::run(const dpp::slashcommand_t &event) {
    const dpp::snowflake targetId = *readUser(event, "user");
    const std::string reason = readReason(event);
    const std::string name = getUserName(event, targetId);

    const std::optional<int64_t> duration = Duration::parse(readString(event, "duration"));
    if (!duration.has_value() || *duration > MAX_TEMPBAN_SECONDS) {
        co_await event.co_reply(makeReply(COLOR_ERROR, "Invalid duration. Use for example `30m`, `12h`, `7d` or "
                                                       "`1w2d`, up to one year.", true));
        co_return;
    }

    const std::string error = co_await checkTarget(event, targetId, false);
    if (!error.empty()) {
        co_await event.co_reply(makeReply(COLOR_ERROR, error, true));
        co_return;
    }

    co_await event.co_thinking(false);

    const std::string notice = "You were banned from **" + getGuildName(event.command.guild_id) + "** for " +
                               Duration::format(*duration) + ".\nReason: " + reason;
    co_await notify(targetId, notice);

    mBot.set_audit_reason(reason + " (" + Duration::format(*duration) + ")");
    const dpp::confirmation_callback_t result = co_await mBot.co_guild_ban_add(event.command.guild_id, targetId, 0);
    if (result.is_error()) {
        co_await event.co_edit_original_response(makeReply(COLOR_ERROR, "Could not ban **" + name + "**: " +
                                                                        result.get_error().human_readable, false));
        co_return;
    }

    mStore.closeActive(targetId, CaseType::Tempban);
    ModerationCase moderationCase = makeCase(event, CaseType::Tempban, targetId, reason);
    moderationCase.mExpiresAt = moderationCase.mCreatedAt + *duration;
    moderationCase.mId = mStore.addCase(moderationCase);
    mLog.recordCase(moderationCase, name);

    co_await event.co_edit_original_response(makeReply(ModerationLog::getColor(CaseType::Tempban),
                                                       "**" + name + "** was banned for " +
                                                       Duration::format(*duration) + ", unbanned " +
                                                       ModerationLog::timestamp(moderationCase.mExpiresAt, 'R') +
                                                       ". Case #" + std::to_string(moderationCase.mId) +
                                                       "\nReason: " + reason, false));
}
