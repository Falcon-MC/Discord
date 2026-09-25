#include "Commands/ContributorCommand.h"

#include <algorithm>
#include <chrono>

namespace {
    constexpr uint32_t COLOR_PENDING = 0xE67E22;
    constexpr uint32_t COLOR_SUCCESS = 0x10B981;
    constexpr uint32_t COLOR_FAILURE = 0xEF4444;
    constexpr uint64_t INTERACTION_LIFETIME = 14 * 60;

    dpp::message makeMessage(uint32_t color, const std::string &title, const std::string &description) {
        dpp::message message;
        message.add_embed(dpp::embed().set_color(color).set_title(title).set_description(description));
        message.set_flags(dpp::m_ephemeral);
        return message;
    }
}

ContributorCommand::PendingGuard::PendingGuard(ContributorCommand &command, dpp::snowflake userId)
    : mCommand(command), mUserId(userId) {
    std::lock_guard<std::mutex> lock(mCommand.mMutex);
    mAcquired = mCommand.mPending.insert(mUserId).second;
}

ContributorCommand::PendingGuard::~PendingGuard() {
    if (!mAcquired)
        return;

    std::lock_guard<std::mutex> lock(mCommand.mMutex);
    mCommand.mPending.erase(mUserId);
}

bool ContributorCommand::PendingGuard::isAcquired() const {
    return mAcquired;
}

ContributorCommand::ContributorCommand(dpp::cluster &bot, GitHubClient &gitHub, const Config &config)
    : mBot(bot), mGitHub(gitHub), mConfig(config) {
}

dpp::slashcommand ContributorCommand::getDefinition() const {
    return dpp::slashcommand("contributor", "Get the Contributor role by linking your GitHub account", mBot.me.id);
}

dpp::task<void> ContributorCommand::execute(dpp::slashcommand_t event) {
    if (_hasRole(event)) {
        co_await event.co_reply(makeMessage(COLOR_SUCCESS, "Already a contributor",
                                            "You already have the Contributor role."));
        co_return;
    }

    PendingGuard guard(*this, event.command.get_issuing_user().id);
    if (!guard.isAcquired()) {
        co_await event.co_reply(makeMessage(COLOR_FAILURE, "Verification in progress",
                                            "Finish the verification you already started, or wait for its code "
                                            "to expire."));
        co_return;
    }

    co_await event.co_thinking(true);

    const std::optional<DeviceCode> code = co_await mGitHub.requestDeviceCode();
    if (!code.has_value()) {
        co_await event.co_edit_original_response(makeMessage(COLOR_FAILURE, "GitHub is unavailable",
                                                              "Could not reach GitHub, try again later."));
        co_return;
    }

    const std::optional<std::string> login = co_await _authorize(event, *code);
    if (!login.has_value())
        co_return;

    co_await _grant(event, *login);
}

bool ContributorCommand::_hasRole(const dpp::slashcommand_t &event) const {
    const std::vector<dpp::snowflake> &roles = event.command.member.get_roles();
    return std::find(roles.begin(), roles.end(), mConfig.mContributorRoleId) != roles.end();
}

dpp::task<std::optional<std::string>> ContributorCommand::_authorize(const dpp::slashcommand_t &event,
                                                                     const DeviceCode &code) {
    dpp::message prompt = makeMessage(COLOR_PENDING, "Link your GitHub account",
                                      "1. Open GitHub with the button below\n"
                                      "2. Enter the code **`" + code.mUserCode + "`**\n"
                                      "3. Authorize the app. It can only read your public profile.\n\n"
                                      "This message updates once you are done.");
    prompt.add_component(dpp::component().add_component(
        dpp::component().set_type(dpp::cot_button).set_label("Open GitHub").set_url(code.mVerificationUri)));
    co_await event.co_edit_original_response(prompt);

    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(std::min(code.mExpiresIn, INTERACTION_LIFETIME));
    uint64_t interval = code.mInterval;

    while (std::chrono::steady_clock::now() < deadline) {
        co_await mBot.co_sleep(interval);

        const TokenPoll poll = co_await mGitHub.pollAccessToken(code.mDeviceCode);
        switch (poll.mStatus) {
            case TokenStatus::Pending:
                break;
            case TokenStatus::SlowDown:
                interval = poll.mInterval > interval ? poll.mInterval : interval + 5;
                break;
            case TokenStatus::Granted: {
                const std::optional<std::string> login = co_await mGitHub.fetchLogin(poll.mAccessToken);
                if (!login.has_value()) {
                    co_await event.co_edit_original_response(makeMessage(COLOR_FAILURE, "GitHub is unavailable",
                                                                          "Could not read your GitHub profile, "
                                                                          "try again later."));
                    co_return std::nullopt;
                }

                const Contribution contribution = co_await mGitHub.findContribution(*login, poll.mAccessToken);
                if (contribution == Contribution::Found)
                    co_return login;

                if (contribution == Contribution::Unknown) {
                    co_await event.co_edit_original_response(makeMessage(COLOR_FAILURE, "GitHub is unavailable",
                                                                          "Could not search your contributions, "
                                                                          "try again in a minute."));
                    co_return std::nullopt;
                }

                co_await event.co_edit_original_response(makeMessage(
                    COLOR_FAILURE, "No contribution found",
                    "No merged pull request or commit from **" + *login + "** was found in the public " +
                    mGitHub.getOrganization() + " repositories.\n\nRun `/contributor` again once one of your "
                    "pull requests is merged."));
                co_return std::nullopt;
            }
            case TokenStatus::Denied:
                co_await event.co_edit_original_response(makeMessage(COLOR_FAILURE, "Authorization cancelled",
                                                                      "You declined the authorization on GitHub."));
                co_return std::nullopt;
            case TokenStatus::Expired:
                co_await event.co_edit_original_response(makeMessage(COLOR_FAILURE, "Code expired",
                                                                      "Run `/contributor` again to get a new code."));
                co_return std::nullopt;
            case TokenStatus::Failed:
                co_await event.co_edit_original_response(makeMessage(COLOR_FAILURE, "GitHub is unavailable",
                                                                      "Could not reach GitHub, try again later."));
                co_return std::nullopt;
        }
    }

    co_await event.co_edit_original_response(makeMessage(COLOR_FAILURE, "Code expired",
                                                          "Run `/contributor` again to get a new code."));
    co_return std::nullopt;
}

dpp::task<void> ContributorCommand::_grant(const dpp::slashcommand_t &event, const std::string &login) {
    const dpp::confirmation_callback_t result = co_await mBot.co_guild_member_add_role(
        event.command.guild_id, event.command.get_issuing_user().id, mConfig.mContributorRoleId);
    if (result.is_error()) {
        mBot.log(dpp::ll_error, "Could not give the Contributor role to " + login + ": " +
                                result.get_error().human_readable);
        co_await event.co_edit_original_response(makeMessage(COLOR_FAILURE, "Could not give the role",
                                                              "Your contributions were found, but the role could "
                                                              "not be given. Ask a maintainer."));
        co_return;
    }

    co_await event.co_edit_original_response(makeMessage(COLOR_SUCCESS, "Welcome, contributor",
                                                          "Thanks for contributing to Falcon, **" + login +
                                                          "**! You now have the Contributor role."));
}
