#pragma once

#include <dpp/snowflake.h>

#include <optional>
#include <string>

struct Config {
    std::string mDiscordToken;
    std::string mGitHubClientId;
    std::string mGitHubOrganization;
    dpp::snowflake mGuildId;
    dpp::snowflake mContributorRoleId;

    static std::optional<Config> fromEnvironment();
};
