#include "Commands/ContributorCommand.h"
#include "Commands/EmbedCommand.h"
#include "Config.h"
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
    bot.on_log(dpp::utility::cout_logger());

    GitHubClient gitHub(bot, config->mGitHubClientId, config->mGitHubOrganization);
    ContributorCommand contributor(bot, gitHub, *config);
    EmbedCommand embed(bot);
    AutoModerator moderator(bot, *config);

    bot.on_message_create([&moderator](const dpp::message_create_t &event) -> dpp::task<void> {
        co_await moderator.onMessageCreate(event);
    });

    bot.on_message_update([&moderator](const dpp::message_update_t &event) -> dpp::task<void> {
        co_await moderator.onMessageUpdate(event);
    });

    bot.on_slashcommand([&contributor, &embed](const dpp::slashcommand_t &event) -> dpp::task<void> {
        const std::string name = event.command.get_command_name();
        if (name == "contributor") {
            co_await contributor.execute(event);
        } else if (name == "embed") {
            co_await embed.execute(event);
        }
    });

    bot.on_ready([&bot, &contributor, &embed, &config](const dpp::ready_t &) {
        if (!dpp::run_once<struct RegisterCommands>())
            return;

        std::vector<dpp::slashcommand> commands;
        commands.push_back(contributor.getDefinition());
        commands.push_back(embed.getDefinition());
        bot.guild_bulk_command_create(commands, config->mGuildId);
        bot.log(dpp::ll_info, "Ready as " + bot.me.username);
    });

    bot.start(dpp::st_wait);
    return 0;
}
