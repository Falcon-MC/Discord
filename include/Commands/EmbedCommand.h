#pragma once

#include "Commands/SlashCommand.h"

#include <dpp/dpp.h>

#include <cstdint>
#include <optional>
#include <string>

class EmbedCommand : public SlashCommand {
public:
    explicit EmbedCommand(dpp::cluster &bot);

    std::string getName() const override;
    dpp::slashcommand getDefinition() const override;
    dpp::task<void> execute(dpp::slashcommand_t event) override;

private:
    dpp::cluster &mBot;

    static bool _isAdministrator(const dpp::slashcommand_t &event);
    static std::string _readString(const dpp::slashcommand_t &event, const std::string &name);
    static std::optional<uint32_t> _parseColor(const std::string &value);
    static std::string _expandLineBreaks(const std::string &value);
};
