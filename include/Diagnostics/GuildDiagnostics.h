#pragma once

#include "Config.h"

#include <dpp/dpp.h>

class GuildDiagnostics {
public:
    GuildDiagnostics(dpp::cluster &bot, const Config &config);

    void logConfiguration() const;
    dpp::task<void> onGuildCreate(dpp::guild_create_t event);

private:
    dpp::cluster &mBot;
    const Config &mConfig;

    void _checkRole(const dpp::guild &guild) const;
    void _checkModLogChannel(const dpp::guild &guild) const;
    dpp::task<void> _checkBotPermissions(dpp::guild guild);
};
