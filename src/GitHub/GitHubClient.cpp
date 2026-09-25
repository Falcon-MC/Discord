#include "GitHub/GitHubClient.h"

#include <utility>

namespace {
    const std::string USER_AGENT = "Falcon-Discord";

    std::optional<dpp::json> parseBody(const dpp::http_request_completion_t &response) {
        if (response.error != dpp::h_success || response.status == 0)
            return std::nullopt;

        dpp::json body = dpp::json::parse(response.body, nullptr, false);
        if (body.is_discarded() || !body.is_object())
            return std::nullopt;

        return body;
    }

    std::string readString(const dpp::json &body, const char *key) {
        const auto it = body.find(key);
        if (it == body.end() || !it->is_string())
            return {};

        return it->get<std::string>();
    }

    uint64_t readUnsigned(const dpp::json &body, const char *key, uint64_t fallback) {
        const auto it = body.find(key);
        if (it == body.end() || !it->is_number_unsigned())
            return fallback;

        return it->get<uint64_t>();
    }
}

GitHubClient::GitHubClient(dpp::cluster &bot, std::string clientId, std::string organization)
    : mBot(bot), mClientId(std::move(clientId)), mOrganization(std::move(organization)) {
}

const std::string &GitHubClient::getOrganization() const {
    return mOrganization;
}

dpp::task<std::optional<DeviceCode>> GitHubClient::requestDeviceCode() {
    const std::optional<dpp::json> body = co_await _post("https://github.com/login/device/code",
                                                         dpp::json{{"client_id", mClientId}});
    if (!body.has_value())
        co_return std::nullopt;

    DeviceCode code;
    code.mDeviceCode = readString(*body, "device_code");
    code.mUserCode = readString(*body, "user_code");
    code.mVerificationUri = readString(*body, "verification_uri");
    code.mExpiresIn = readUnsigned(*body, "expires_in", 900);
    code.mInterval = readUnsigned(*body, "interval", 5);
    if (code.mDeviceCode.empty() || code.mUserCode.empty() || code.mVerificationUri.empty())
        co_return std::nullopt;

    co_return code;
}

dpp::task<TokenPoll> GitHubClient::pollAccessToken(std::string deviceCode) {
    const std::optional<dpp::json> body = co_await _post("https://github.com/login/oauth/access_token", dpp::json{
        {"client_id", mClientId},
        {"device_code", deviceCode},
        {"grant_type", "urn:ietf:params:oauth:grant-type:device_code"}
    });

    TokenPoll poll;
    if (!body.has_value())
        co_return poll;

    poll.mAccessToken = readString(*body, "access_token");
    if (!poll.mAccessToken.empty()) {
        poll.mStatus = TokenStatus::Granted;
        co_return poll;
    }

    const std::string error = readString(*body, "error");
    if (error == "authorization_pending") {
        poll.mStatus = TokenStatus::Pending;
    } else if (error == "slow_down") {
        poll.mStatus = TokenStatus::SlowDown;
        poll.mInterval = readUnsigned(*body, "interval", 0);
    } else if (error == "access_denied") {
        poll.mStatus = TokenStatus::Denied;
    } else if (error == "expired_token") {
        poll.mStatus = TokenStatus::Expired;
    }
    co_return poll;
}

dpp::task<std::optional<std::string>> GitHubClient::fetchLogin(std::string accessToken) {
    const std::optional<dpp::json> body = co_await _get("https://api.github.com/user", accessToken);
    if (!body.has_value())
        co_return std::nullopt;

    std::string login = readString(*body, "login");
    if (login.empty())
        co_return std::nullopt;

    co_return login;
}

dpp::task<Contribution> GitHubClient::findContribution(std::string login, std::string accessToken) {
    const std::string scope = "org:" + mOrganization + " author:" + login + " is:public";

    const std::optional<uint64_t> pullRequests = co_await _count("issues", "is:pr is:merged " + scope, accessToken);
    if (pullRequests.has_value() && *pullRequests > 0)
        co_return Contribution::Found;

    const std::optional<uint64_t> commits = co_await _count("commits", scope, accessToken);
    if (commits.has_value() && *commits > 0)
        co_return Contribution::Found;

    if (!pullRequests.has_value() || !commits.has_value())
        co_return Contribution::Unknown;

    co_return Contribution::NotFound;
}

dpp::task<std::optional<dpp::json>> GitHubClient::_post(std::string url, dpp::json body) {
    const dpp::http_request_completion_t response = co_await mBot.co_request(url, dpp::m_post, body.dump(),
                                                                             "application/json", {
        {"Accept", "application/json"},
        {"User-Agent", USER_AGENT}
    });
    if (response.status != 200)
        co_return std::nullopt;

    co_return parseBody(response);
}

dpp::task<std::optional<dpp::json>> GitHubClient::_get(std::string url, std::string accessToken) {
    const dpp::http_request_completion_t response = co_await mBot.co_request(url, dpp::m_get, "", "", {
        {"Accept", "application/vnd.github+json"},
        {"Authorization", "Bearer " + accessToken},
        {"User-Agent", USER_AGENT},
        {"X-GitHub-Api-Version", "2022-11-28"}
    });
    if (response.status != 200)
        co_return std::nullopt;

    co_return parseBody(response);
}

dpp::task<std::optional<uint64_t>> GitHubClient::_count(std::string endpoint, std::string query,
                                                        std::string accessToken) {
    const std::string url = "https://api.github.com/search/" + endpoint + "?per_page=1&q=" +
                            dpp::utility::url_encode(query);
    const std::optional<dpp::json> body = co_await _get(url, accessToken);
    if (!body.has_value())
        co_return std::nullopt;

    const auto it = body->find("total_count");
    if (it == body->end() || !it->is_number_unsigned())
        co_return std::nullopt;

    co_return it->get<uint64_t>();
}
