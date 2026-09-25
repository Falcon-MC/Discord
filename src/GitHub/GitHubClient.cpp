#include "GitHub/GitHubClient.h"

#include <map>
#include <utility>

namespace {
    const std::string USER_AGENT = "Falcon-Discord";
    const std::string DEVICE_CODE_URL = "https://github.com/login/device/code";
    const std::string ACCESS_TOKEN_URL = "https://github.com/login/oauth/access_token";
    const std::string USER_URL = "https://api.github.com/user";
    const std::string SEARCH_ISSUES = "issues";
    const std::string SEARCH_COMMITS = "commits";

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
    dpp::json request = dpp::json::object();
    request["client_id"] = mClientId;

    const std::optional<dpp::json> body = co_await _post(DEVICE_CODE_URL, request);
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
    dpp::json request = dpp::json::object();
    request["client_id"] = mClientId;
    request["device_code"] = deviceCode;
    request["grant_type"] = "urn:ietf:params:oauth:grant-type:device_code";

    const std::optional<dpp::json> body = co_await _post(ACCESS_TOKEN_URL, request);

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
    const std::optional<dpp::json> body = co_await _get(USER_URL, accessToken);
    if (!body.has_value())
        co_return std::nullopt;

    std::string login = readString(*body, "login");
    if (login.empty())
        co_return std::nullopt;

    co_return login;
}

dpp::task<Contribution> GitHubClient::findContribution(std::string login, std::string accessToken) {
    const std::string scope = "org:" + mOrganization + " author:" + login + " is:public";
    const std::string pullRequestQuery = "is:pr is:merged " + scope;

    const std::optional<uint64_t> pullRequests = co_await _count(SEARCH_ISSUES, pullRequestQuery, accessToken);
    if (pullRequests.has_value() && *pullRequests > 0)
        co_return Contribution::Found;

    const std::optional<uint64_t> commits = co_await _count(SEARCH_COMMITS, scope, accessToken);
    if (commits.has_value() && *commits > 0)
        co_return Contribution::Found;

    if (!pullRequests.has_value() || !commits.has_value())
        co_return Contribution::Unknown;

    co_return Contribution::NotFound;
}

dpp::task<std::optional<dpp::json>> GitHubClient::_post(std::string url, dpp::json body) {
    std::multimap<std::string, std::string> headers;
    headers.emplace("Accept", "application/json");
    headers.emplace("User-Agent", USER_AGENT);

    const std::string payload = body.dump();
    const std::string mimeType = "application/json";
    const dpp::http_request_completion_t response = co_await mBot.co_request(url, dpp::m_post, payload, mimeType,
                                                                             headers);
    if (response.status != 200)
        co_return std::nullopt;

    co_return parseBody(response);
}

dpp::task<std::optional<dpp::json>> GitHubClient::_get(std::string url, std::string accessToken) {
    std::multimap<std::string, std::string> headers;
    headers.emplace("Accept", "application/vnd.github+json");
    headers.emplace("Authorization", "Bearer " + accessToken);
    headers.emplace("User-Agent", USER_AGENT);
    headers.emplace("X-GitHub-Api-Version", "2022-11-28");

    const std::string empty;
    const dpp::http_request_completion_t response = co_await mBot.co_request(url, dpp::m_get, empty, empty,
                                                                             headers);
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
