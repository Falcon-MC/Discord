#pragma once

#include "Config.h"
#include "Moderation/ModerationStore.h"

#include <dpp/dpp.h>

#include <string>

class ModerationLog {
public:
    ModerationLog(dpp::cluster &bot, const Config &config);

    void recordCase(const ModerationCase &moderationCase, const std::string &userName) const;
    void recordEvent(const std::string &title, dpp::snowflake userId, const std::string &userName,
                     dpp::snowflake channelId, const std::string &details) const;

    static uint32_t getColor(CaseType type);
    static std::string getTitle(CaseType type);
    static std::string mention(dpp::snowflake id);
    static std::string timestamp(int64_t epoch, char style);

private:
    dpp::cluster &mBot;
    const Config &mConfig;

    void _send(const dpp::embed &embed) const;
};
