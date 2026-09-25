#pragma once

#include "Config.h"
#include "GitHub/GitHubClient.h"

#include <dpp/dpp.h>

#include <mutex>
#include <string>
#include <unordered_set>

class ContributorCommand {
public:
    ContributorCommand(dpp::cluster &bot, GitHubClient &gitHub, const Config &config);

    dpp::slashcommand getDefinition() const;
    dpp::task<void> execute(dpp::slashcommand_t event);

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
