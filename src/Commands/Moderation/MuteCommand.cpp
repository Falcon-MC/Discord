#include "Commands/Moderation/MuteCommand.h"

#include "Moderation/Duration.h"

#include <ctime>

namespace {
    constexpr int64_t MAX_TIMEOUT_SECONDS = 28LL * 24 * 60 * 60;
}

MuteCommand::MuteCommand(dpp::cluster &bot, ModerationStore &store, ModerationLog &log)
    : ModerationCommand(bot, store, log, dpp::p_moderate_members) {
}

std::string MuteCommand::getName() const {
    return "mute";
}

dpp::slashcommand MuteCommand::getDefinition() const {
    dpp::slashcommand command = makeDefinition("Time out a member so they cannot talk");
    command.add_option(dpp::command_option(dpp::co_user, "user", "Member to mute", true));
    command.add_option(dpp::command_option(dpp::co_string, "duration", "Duration, e.g. 10m, 2h, 1d, up to 28d", true)
                           .set_max_length(20));
    command.add_option(dpp::command_option(dpp::co_string, "reason", "Reason, shown in the logs").set_max_length(500));
    return command;
}

dpp::task<void> MuteCommand::run(const dpp::slashcommand_t &event) {
    const dpp::snowflake targetId = *readUser(event, "user");
    const std::string reason = readReason(event);
    const std::string name = getUserName(event, targetId);

    const std::optional<int64_t> duration = Duration::parse(readString(event, "duration"));
    if (!duration.has_value() || *duration > MAX_TIMEOUT_SECONDS) {
        co_await event.co_reply(makeReply(COLOR_ERROR, "Invalid duration. Use for example `10m`, `2h` or `1d`, up "
                                                       "to 28 days.", true));
        co_return;
    }

    const std::string error = co_await checkTarget(event, targetId, true);
    if (!error.empty()) {
        co_await event.co_reply(makeReply(COLOR_ERROR, error, true));
        co_return;
    }

    co_await event.co_thinking(false);

    ModerationCase moderationCase = makeCase(event, CaseType::Mute, targetId, reason);
    moderationCase.mExpiresAt = moderationCase.mCreatedAt + *duration;

    mBot.set_audit_reason(reason);
    const dpp::confirmation_callback_t result = co_await mBot.co_guild_member_timeout(
        event.command.guild_id, targetId, static_cast<time_t>(moderationCase.mExpiresAt));
    if (result.is_error()) {
        co_await event.co_edit_original_response(makeReply(COLOR_ERROR, "Could not mute **" + name + "**: " +
                                                                        result.get_error().human_readable, false));
        co_return;
    }

    mStore.closeActive(targetId, CaseType::Mute);
    moderationCase.mId = mStore.addCase(moderationCase);
    mLog.recordCase(moderationCase, name);

    const std::string notice = "You were muted in **" + getGuildName(event.command.guild_id) + "** for " +
                               Duration::format(*duration) + ".\nReason: " + reason;
    co_await notify(targetId, notice);

    co_await event.co_edit_original_response(makeReply(ModerationLog::getColor(CaseType::Mute),
                                                       "**" + name + "** was muted for " +
                                                       Duration::format(*duration) + ". Case #" +
                                                       std::to_string(moderationCase.mId) + "\nReason: " + reason,
                                                       false));
}
