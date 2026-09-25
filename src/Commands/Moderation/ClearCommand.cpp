#include "Commands/Moderation/ClearCommand.h"

#include <algorithm>
#include <ctime>

namespace {
    constexpr double BULK_DELETE_MAX_AGE = 14.0 * 24 * 60 * 60 - 60;
}

ClearCommand::ClearCommand(dpp::cluster &bot, ModerationStore &store, ModerationLog &log)
    : ModerationCommand(bot, store, log, dpp::p_manage_messages) {
}

std::string ClearCommand::getName() const {
    return "clear";
}

dpp::slashcommand ClearCommand::getDefinition() const {
    dpp::slashcommand command = makeDefinition("Delete the latest messages of this channel");
    command.add_option(dpp::command_option(dpp::co_integer, "amount", "Number of messages to delete (1 to 100)", true)
                           .set_min_value(1)
                           .set_max_value(100));
    command.add_option(dpp::command_option(dpp::co_user, "user", "Only delete the messages of this user"));
    return command;
}

dpp::task<void> ClearCommand::run(const dpp::slashcommand_t &event) {
    const int64_t amount = readInteger(event, "amount").value_or(0);
    const std::optional<dpp::snowflake> filter = readUser(event, "user");
    const dpp::snowflake channelId = event.command.channel_id;

    co_await event.co_thinking(true);

    const dpp::snowflake none;
    const dpp::confirmation_callback_t fetched = co_await mBot.co_messages_get(channelId, none, none, none, 100);
    if (fetched.is_error()) {
        co_await event.co_edit_original_response(makeReply(COLOR_ERROR, "Could not read the messages: " +
                                                                        fetched.get_error().human_readable, true));
        co_return;
    }

    const dpp::message_map messages = fetched.get<dpp::message_map>();
    std::vector<dpp::snowflake> ids;
    for (const auto &[id, message] : messages) {
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end(), [](const dpp::snowflake &left, const dpp::snowflake &right) {
        return static_cast<uint64_t>(left) > static_cast<uint64_t>(right);
    });

    const double now = static_cast<double>(std::time(nullptr));
    std::vector<dpp::snowflake> selected;
    size_t skippedOld = 0;
    for (const dpp::snowflake &id : ids) {
        if (static_cast<int64_t>(selected.size()) >= amount)
            break;

        const dpp::message &message = messages.at(id);
        if (message.pinned)
            continue;
        if (filter.has_value() && message.author.id != *filter)
            continue;
        if (now - id.get_creation_time() > BULK_DELETE_MAX_AGE) {
            ++skippedOld;
            continue;
        }
        selected.push_back(id);
    }

    if (selected.empty()) {
        co_await event.co_edit_original_response(makeReply(COLOR_INFO, "No message to delete. Messages older than "
                                                                       "14 days cannot be cleared.", true));
        co_return;
    }

    mBot.set_audit_reason("Cleared by " + event.command.get_issuing_user().username);
    dpp::confirmation_callback_t result;
    if (selected.size() == 1) {
        result = co_await mBot.co_message_delete(selected.front(), channelId);
    } else {
        result = co_await mBot.co_message_delete_bulk(selected, channelId);
    }

    if (result.is_error()) {
        co_await event.co_edit_original_response(makeReply(COLOR_ERROR, "Could not delete the messages: " +
                                                                        result.get_error().human_readable, true));
        co_return;
    }

    const dpp::user &moderator = event.command.get_issuing_user();
    std::string details = std::to_string(selected.size()) + " messages deleted by " + moderator.username;
    if (filter.has_value())
        details += " from " + ModerationLog::mention(*filter);
    mLog.recordEvent("Messages cleared", moderator.id, moderator.username, channelId, details);

    std::string text = "Deleted " + std::to_string(selected.size()) + " message" +
                       (selected.size() == 1 ? "" : "s") + ".";
    if (skippedOld > 0)
        text += " Messages older than 14 days were skipped.";
    co_await event.co_edit_original_response(makeReply(COLOR_INFO, text, true));
}
