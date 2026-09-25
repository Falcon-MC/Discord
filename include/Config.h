#pragma once

#include <dpp/snowflake.h>

#include <optional>
#include <string>

struct Config {
    std::string mDiscordToken;
    std::string mGitHubClientId;
    std::string mGitHubOrganization;
    std::string mDataDirectory;
    dpp::snowflake mGuildId;
    dpp::snowflake mContributorRoleId;
    dpp::snowflake mModLogChannelId;

    static std::optional<Config> fromEnvironment();
};
