#include "Commands/ContributorCommand.h"
#include "Commands/EmbedCommand.h"
#include "Commands/Moderation/BanCommand.h"
#include "Commands/Moderation/ClearCommand.h"
#include "Commands/Moderation/ClearWarningsCommand.h"
#include "Commands/Moderation/HistoryCommand.h"
#include "Commands/Moderation/KickCommand.h"
#include "Commands/Moderation/MuteCommand.h"
#include "Commands/Moderation/TempbanCommand.h"
#include "Commands/Moderation/UnbanCommand.h"
#include "Commands/Moderation/UnmuteCommand.h"
#include "Commands/Moderation/WarnCommand.h"
#include "Commands/Moderation/WarningsCommand.h"
#include "Commands/SlashCommand.h"
#include "Config.h"
#include "Diagnostics/ConsoleLogger.h"
#include "Diagnostics/GuildDiagnostics.h"
#include "GitHub/GitHubClient.h"
#include "Moderation/AutoModerator.h"
#include "Moderation/ModerationLog.h"
#include "Moderation/ModerationStore.h"
#include "Moderation/TempbanScheduler.h"

#include <dpp/dpp.h>

#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

int main() {
    const std::optional<Config> config = Config::fromEnvironment();
    if (!config.has_value())
        return 1;

    ModerationStore store(std::filesystem::path(config->mDataDirectory) / "moderation.json");
    std::string storeError;
    if (!store.load(storeError)) {
        std::cerr << storeError << std::endl;
        return 1;
    }

    dpp::cluster bot(config->mDiscordToken, dpp::i_guilds | dpp::i_guild_messages | dpp::i_message_content);
    bot.on_log(makeConsoleLogger());

    GitHubClient gitHub(bot, config->mGitHubClientId, config->mGitHubOrganization);
    ModerationLog moderationLog(bot, *config);
    AutoModerator moderator(bot, *config, store, moderationLog);
    TempbanScheduler tempbans(bot, *config, store, moderationLog);
    GuildDiagnostics diagnostics(bot, *config);

    std::vector<std::unique_ptr<SlashCommand>> commands;
    commands.push_back(std::make_unique<ContributorCommand>(bot, gitHub, *config));
    commands.push_back(std::make_unique<EmbedCommand>(bot));
    commands.push_back(std::make_unique<BanCommand>(bot, store, moderationLog));
    commands.push_back(std::make_unique<TempbanCommand>(bot, store, moderationLog));
    commands.push_back(std::make_unique<UnbanCommand>(bot, store, moderationLog));
    commands.push_back(std::make_unique<KickCommand>(bot, store, moderationLog));
    commands.push_back(std::make_unique<MuteCommand>(bot, store, moderationLog));
    commands.push_back(std::make_unique<UnmuteCommand>(bot, store, moderationLog));
    commands.push_back(std::make_unique<WarnCommand>(bot, store, moderationLog));
    commands.push_back(std::make_unique<WarningsCommand>(bot, store, moderationLog));
    commands.push_back(std::make_unique<ClearWarningsCommand>(bot, store, moderationLog));
    commands.push_back(std::make_unique<HistoryCommand>(bot, store, moderationLog));
    commands.push_back(std::make_unique<ClearCommand>(bot, store, moderationLog));

    std::unordered_map<std::string, SlashCommand *> commandsByName;
    for (const std::unique_ptr<SlashCommand> &command : commands) {
        commandsByName[command->getName()] = command.get();
    }

    diagnostics.logConfiguration();
    bot.log(dpp::ll_info, "Loaded " + std::to_string(store.getCaseCount()) + " moderation cases from " +
                          store.getPath().string());
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

    bot.on_slashcommand([&bot, &commandsByName](const dpp::slashcommand_t &event) -> dpp::task<void> {
        const std::string name = event.command.get_command_name();
        const auto it = commandsByName.find(name);
        if (it == commandsByName.end())
            co_return;

        bot.log(dpp::ll_info, "/" + name + " used by " + event.command.get_issuing_user().username);
        co_await it->second->execute(event);
    });

    bot.on_ready([&bot, &commands, &config, &tempbans](const dpp::ready_t &) {
        if (!dpp::run_once<struct RegisterCommands>())
            return;

        bot.log(dpp::ll_info, "Connected as " + bot.me.username + " (" + bot.me.id.str() + ")");

        std::vector<dpp::slashcommand> definitions;
        for (const std::unique_ptr<SlashCommand> &command : commands) {
            definitions.push_back(command->getDefinition());
        }

        const size_t count = definitions.size();
        bot.guild_bulk_command_create(definitions, config->mGuildId,
                                      [&bot, count](const dpp::confirmation_callback_t &result) {
            if (result.is_error()) {
                bot.log(dpp::ll_error, "Could not register the slash commands: " +
                                       result.get_error().human_readable);
                return;
            }

            bot.log(dpp::ll_info, "Registered " + std::to_string(count) + " slash commands");
        });

        tempbans.start();
        bot.log(dpp::ll_info, "Ready as " + bot.me.username);
    });

    bot.start(dpp::st_wait);
    return 0;
}
