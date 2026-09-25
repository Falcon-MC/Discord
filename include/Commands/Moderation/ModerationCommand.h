#pragma once

#include "Commands/SlashCommand.h"
#include "Moderation/ModerationLog.h"
#include "Moderation/ModerationStore.h"

#include <dpp/dpp.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class ModerationCommand : public SlashCommand {
public:
    dpp::task<void> execute(dpp::slashcommand_t event) override;

protected:
    ModerationCommand(dpp::cluster &bot, ModerationStore &store, ModerationLog &log, uint64_t permission);

    dpp::cluster &mBot;
    ModerationStore &mStore;
    ModerationLog &mLog;

    virtual dpp::task<void> run(const dpp::slashcommand_t &event) = 0;

    dpp::slashcommand makeDefinition(const std::string &description) const;
    dpp::task<std::string> checkTarget(const dpp::slashcommand_t &event, dpp::snowflake targetId,
                                       bool mustBeMember);
    dpp::task<void> notify(dpp::snowflake userId, std::string text);
    ModerationCase makeCase(const dpp::slashcommand_t &event, CaseType type, dpp::snowflake targetId,
                            const std::string &reason) const;

    static dpp::message makeReply(uint32_t color, const std::string &text, bool ephemeral);
    static std::string readString(const dpp::slashcommand_t &event, const std::string &name);
    static std::optional<int64_t> readInteger(const dpp::slashcommand_t &event, const std::string &name);
    static std::optional<dpp::snowflake> readUser(const dpp::slashcommand_t &event, const std::string &name);
    static std::string readReason(const dpp::slashcommand_t &event);
    static std::string getUserName(const dpp::slashcommand_t &event, dpp::snowflake userId);
    static std::string getGuildName(dpp::snowflake guildId);

    static constexpr uint32_t COLOR_ERROR = 0xEF4444;
    static constexpr uint32_t COLOR_INFO = 0x3B82F6;

private:
    uint64_t mPermission;

    static uint8_t _getTopPosition(const std::vector<dpp::snowflake> &roles);
};
