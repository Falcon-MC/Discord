#pragma once

#include "Commands/SlashCommand.h"
#include "Config.h"
#include "GitHub/GitHubClient.h"

#include <dpp/dpp.h>

#include <mutex>
#include <string>
#include <unordered_set>

class ContributorCommand : public SlashCommand {
public:
    ContributorCommand(dpp::cluster &bot, GitHubClient &gitHub, const Config &config);

    std::string getName() const override;
    dpp::slashcommand getDefinition() const override;
    dpp::task<void> execute(dpp::slashcommand_t event) override;

private:
    class PendingGuard {
    public:
        PendingGuard(ContributorCommand &command, dpp::snowflake userId);
        ~PendingGuard();

        PendingGuard(const PendingGuard &) = delete;
        PendingGuard &operator=(const PendingGuard &) = delete;

        bool isAcquired() const;

    private:
        ContributorCommand &mCommand;
        dpp::snowflake mUserId;
        bool mAcquired;
    };

    dpp::cluster &mBot;
    GitHubClient &mGitHub;
    const Config &mConfig;
    std::mutex mMutex;
    std::unordered_set<dpp::snowflake> mPending;

    bool _hasRole(const dpp::slashcommand_t &event) const;
    dpp::task<std::optional<std::string>> _authorize(const dpp::slashcommand_t &event, const DeviceCode &code);
    dpp::task<void> _grant(const dpp::slashcommand_t &event, const std::string &login);
};
