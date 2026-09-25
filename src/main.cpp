#include "Commands/ContributorCommand.h"
#include "Config.h"
#include "GitHub/GitHubClient.h"

#include <dpp/dpp.h>

#include <optional>

int main() {
    const std::optional<Config> config = Config::fromEnvironment();
    if (!config.has_value())
        return 1;

    dpp::cluster bot(config->mDiscordToken, dpp::i_guilds);
    bot.on_log(dpp::utility::cout_logger());

    GitHubClient gitHub(bot, config->mGitHubClientId, config->mGitHubOrganization);
    ContributorCommand contributor(bot, gitHub, *config);

    bot.on_slashcommand([&contributor](const dpp::slashcommand_t &event) -> dpp::task<void> {
        if (event.command.get_command_name() == "contributor")
            co_await contributor.execute(event);
    });

    bot.on_ready([&bot, &contributor, &config](const dpp::ready_t &) {
        if (!dpp::run_once<struct RegisterCommands>())
            return;

        bot.guild_bulk_command_create({contributor.getDefinition()}, config->mGuildId);
        bot.log(dpp::ll_info, "Ready as " + bot.me.username);
    });

    bot.start(dpp::st_wait);
    return 0;
}
