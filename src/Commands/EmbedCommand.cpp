#include "Commands/EmbedCommand.h"

#include <cctype>
#include <variant>

namespace {
    constexpr uint32_t DEFAULT_COLOR = 0xE67E22;
    constexpr uint32_t COLOR_FAILURE = 0xEF4444;
    constexpr uint32_t COLOR_SUCCESS = 0x10B981;

    dpp::message makeReply(uint32_t color, const std::string &description) {
        dpp::message message;
        message.add_embed(dpp::embed().set_color(color).set_description(description));
        message.set_flags(dpp::m_ephemeral);
        return message;
    }
}

EmbedCommand::EmbedCommand(dpp::cluster &bot)
    : mBot(bot) {
}

std::string EmbedCommand::getName() const {
    return "embed";
}

dpp::slashcommand EmbedCommand::getDefinition() const {
    dpp::slashcommand command("embed", "Send an embed in this channel", mBot.me.id);
    command.set_default_permissions(dpp::p_administrator);
    command.add_option(dpp::command_option(dpp::co_string, "title", "Title of the embed", true)
                           .set_max_length(256));
    command.add_option(dpp::command_option(dpp::co_string, "description", "Text of the embed, \\n for a new line",
                                           true)
                           .set_max_length(4000));
    command.add_option(dpp::command_option(dpp::co_string, "color", "Hex color, e.g. #E67E22")
                           .set_max_length(7));
    return command;
}

dpp::task<void> EmbedCommand::execute(dpp::slashcommand_t event) {
    if (!_isAdministrator(event)) {
        co_await event.co_reply(makeReply(COLOR_FAILURE, "Only administrators can use this command."));
        co_return;
    }

    uint32_t color = DEFAULT_COLOR;
    const std::string colorText = _readString(event, "color");
    if (!colorText.empty()) {
        const std::optional<uint32_t> parsed = _parseColor(colorText);
        if (!parsed.has_value()) {
            co_await event.co_reply(makeReply(COLOR_FAILURE, "`" + colorText + "` is not a hex color such as "
                                                             "`#E67E22`."));
            co_return;
        }
        color = *parsed;
    }

    dpp::embed embed;
    embed.set_color(color);
    embed.set_title(_readString(event, "title"));
    embed.set_description(_expandLineBreaks(_readString(event, "description")));

    const dpp::message message(event.command.channel_id, embed);
    const dpp::confirmation_callback_t result = co_await mBot.co_message_create(message);
    if (result.is_error()) {
        co_await event.co_reply(makeReply(COLOR_FAILURE, "Could not send the embed: " +
                                                         result.get_error().human_readable));
        co_return;
    }

    co_await event.co_reply(makeReply(COLOR_SUCCESS, "Embed sent."));
}

bool EmbedCommand::_isAdministrator(const dpp::slashcommand_t &event) {
    const auto &permissions = event.command.resolved.member_permissions;
    const auto it = permissions.find(event.command.get_issuing_user().id);
    return it != permissions.end() && it->second.has(dpp::p_administrator);
}

std::string EmbedCommand::_readString(const dpp::slashcommand_t &event, const std::string &name) {
    const dpp::command_value value = event.get_parameter(name);
    if (!std::holds_alternative<std::string>(value))
        return {};

    return std::get<std::string>(value);
}

std::optional<uint32_t> EmbedCommand::_parseColor(const std::string &value) {
    std::string hex = value;
    if (!hex.empty() && hex.front() == '#')
        hex.erase(0, 1);

    if (hex.size() != 6)
        return std::nullopt;

    for (const char character : hex) {
        if (!std::isxdigit(static_cast<unsigned char>(character)))
            return std::nullopt;
    }

    return static_cast<uint32_t>(std::stoul(hex, nullptr, 16));
}

std::string EmbedCommand::_expandLineBreaks(const std::string &value) {
    std::string result;
    result.reserve(value.size());
    for (size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '\\' && index + 1 < value.size() && value[index + 1] == 'n') {
            result += '\n';
            ++index;
        } else {
            result += value[index];
        }
    }
    return result;
}
