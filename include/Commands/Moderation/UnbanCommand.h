#pragma once

#include "Commands/Moderation/ModerationCommand.h"

class UnbanCommand : public ModerationCommand {
public:
    UnbanCommand(dpp::cluster &bot, ModerationStore &store, ModerationLog &log);

    std::string getName() const override;
    dpp::slashcommand getDefinition() const override;

protected:
    dpp::task<void> run(const dpp::slashcommand_t &event) override;
};
