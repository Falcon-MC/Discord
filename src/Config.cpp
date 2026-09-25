#include "Config.h"

#include <cstdlib>
#include <iostream>

namespace {
    std::string readVariable(const char *name, const std::string &fallback = {}) {
        const char *value = std::getenv(name);
        if (value == nullptr || *value == '\0')
            return fallback;

        return value;
    }

    std::optional<dpp::snowflake> readSnowflake(const char *name) {
        const std::string value = readVariable(name);
        if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
            return std::nullopt;

        return dpp::snowflake(std::stoull(value));
    }
}

std::optional<Config> Config::fromEnvironment() {
    Config config;
    config.mDiscordToken = readVariable("DISCORD_TOKEN");
    config.mGitHubClientId = readVariable("GITHUB_CLIENT_ID");
    config.mGitHubOrganization = readVariable("GITHUB_ORGANIZATION", "Falcon-MC");

    bool valid = true;
    if (config.mDiscordToken.empty()) {
        std::cerr << "DISCORD_TOKEN is not set" << std::endl;
        valid = false;
    }
    if (config.mGitHubClientId.empty()) {
        std::cerr << "GITHUB_CLIENT_ID is not set" << std::endl;
        valid = false;
    }

    const std::optional<dpp::snowflake> guildId = readSnowflake("DISCORD_GUILD_ID");
    if (guildId.has_value()) {
        config.mGuildId = *guildId;
    } else {
        std::cerr << "DISCORD_GUILD_ID is not set or is not a valid id" << std::endl;
        valid = false;
    }

    const std::optional<dpp::snowflake> roleId = readSnowflake("DISCORD_CONTRIBUTOR_ROLE_ID");
    if (roleId.has_value()) {
        config.mContributorRoleId = *roleId;
    } else {
        std::cerr << "DISCORD_CONTRIBUTOR_ROLE_ID is not set or is not a valid id" << std::endl;
        valid = false;
    }

    const std::optional<dpp::snowflake> modLogChannelId = readSnowflake("DISCORD_MOD_LOG_CHANNEL_ID");
    if (modLogChannelId.has_value())
        config.mModLogChannelId = *modLogChannelId;

    if (!valid)
        return std::nullopt;

    return config;
}
