#pragma once

#include <dpp/dpp.h>

#include <string>

class SlashCommand {
public:
    virtual ~SlashCommand() = default;

    virtual std::string getName() const = 0;
    virtual dpp::slashcommand getDefinition() const = 0;
    virtual dpp::task<void> execute(dpp::slashcommand_t event) = 0;
};
