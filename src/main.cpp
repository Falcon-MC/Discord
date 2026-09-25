#include "Commands/ContributorCommand.h"
#include "Commands/EmbedCommand.h"
#include "Config.h"
#include "Diagnostics/ConsoleLogger.h"
#include "Diagnostics/GuildDiagnostics.h"
#include "GitHub/GitHubClient.h"
#include "Moderation/AutoModerator.h"

#include <dpp/dpp.h>

#include <optional>
#include <string>
#include <vector>

int main() {
    const std::optional<Config> config = Config::fromEnvironment();
    if (!config.has_value())
        return 1;

    dpp::cluster bot(config->mDiscordToken, dpp::i_guilds | dpp::i_guild_messages | dpp::i_message_content);
    bot.on_log(makeConsoleLogger());

    GitHubClient gitHub(bot, config->mGitHubClientId, config->mGitHubOrganization);
    ContributorCommand contributor(bot, gitHub, *config);
    EmbedCommand embed(bot);
    AutoModerator moderator(bot, *config);
    GuildDiagnostics diagnostics(bot, *config);

    diagnostics.logConfiguration();
    bot.log(dpp::ll_info, "Connecting to Discord");

    bot.on_guild_create([&diagnostics](const dpp::guild_create_t &event) -> dpp::task<void> {
        co_await diagnostics.onGuildCreate(event);
    });

    bot.on_message_create([&moderator](const dpp::message_create_t &event) -> dpp::task<void> {
        co_await moderator.onMessageCreate(event);
    });

    bot.on_message_update([&moderator](const dpp::message_update_t &event) -> dpp::task<void> {
        co_await moderator.onMessageUpdate(event);
    });

    bot.on_slashcommand([&bot, &contributor, &embed](const dpp::slashcommand_t &event) -> dpp::task<void> {
        const std::string name = event.command.get_command_name();
        bot.log(dpp::ll_info, "/" + name + " used by " + event.command.get_issuing_user().username);
        if (name == "contributor") {
            co_await contributor.execute(event);
        } else if (name == "embed") {
            co_await embed.execute(event);
        }
    });

    bot.on_ready([&bot, &contributor, &embed, &config](const dpp::ready_t &) {
        if (!dpp::run_once<struct RegisterCommands>())
            return;

        bot.log(dpp::ll_info, "Connected as " + bot.me.username + " (" + bot.me.id.str() + ")");

        std::vector<dpp::slashcommand> commands;
        commands.push_back(contributor.getDefinition());
        commands.push_back(embed.getDefinition());
        const size_t count = commands.size();
        bot.guild_bulk_command_create(commands, config->mGuildId,
                                      [&bot, count](const dpp::confirmation_callback_t &result) {
            if (result.is_error()) {
                bot.log(dpp::ll_error, "Could not register the slash commands: " +
                                       result.get_error().human_readable);
                return;
            }

            bot.log(dpp::ll_info, "Registered " + std::to_string(count) + " slash commands");
        });
        bot.log(dpp::ll_info, "Ready as " + bot.me.username);
    });

    bot.start(dpp::st_wait);
    return 0;
}
