#pragma once

#include <dpp/dpp.h>

#include <cstdint>
#include <optional>
#include <string>

struct DeviceCode {
    std::string mDeviceCode;
    std::string mUserCode;
    std::string mVerificationUri;
    uint64_t mExpiresIn = 0;
    uint64_t mInterval = 5;
};

enum class TokenStatus {
    Pending,
    SlowDown,
    Granted,
    Denied,
    Expired,
    Failed
};

struct TokenPoll {
    TokenStatus mStatus = TokenStatus::Failed;
    std::string mAccessToken;
    uint64_t mInterval = 0;
};

enum class Contribution {
    Found,
    NotFound,
    Unknown
};

class GitHubClient {
public:
    GitHubClient(dpp::cluster &bot, std::string clientId, std::string organization);

    const std::string &getOrganization() const;

    dpp::task<std::optional<DeviceCode>> requestDeviceCode();
    dpp::task<TokenPoll> pollAccessToken(std::string deviceCode);
    dpp::task<std::optional<std::string>> fetchLogin(std::string accessToken);
    dpp::task<Contribution> findContribution(std::string login, std::string accessToken);

private:
    dpp::cluster &mBot;
    std::string mClientId;
    std::string mOrganization;

    dpp::task<std::optional<dpp::json>> _post(std::string url, dpp::json body);
    dpp::task<std::optional<dpp::json>> _get(std::string url, std::string accessToken);
    dpp::task<std::optional<uint64_t>> _count(std::string endpoint, std::string query, std::string accessToken);
};
