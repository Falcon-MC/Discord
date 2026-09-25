#pragma once

#include <dpp/dpp.h>

#include <cstdint>
#include <optional>
#include <string>

class EmbedCommand {
public:
    explicit EmbedCommand(dpp::cluster &bot);

    dpp::slashcommand getDefinition() const;
    dpp::task<void> execute(dpp::slashcommand_t event);

private:
    dpp::cluster &mBot;

    static bool _isAdministrator(const dpp::slashcommand_t &event);
    static std::string _readString(const dpp::slashcommand_t &event, const std::string &name);
    static std::optional<uint32_t> _parseColor(const std::string &value);
    static std::string _expandLineBreaks(const std::string &value);
};
