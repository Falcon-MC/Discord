#include "Moderation/TempbanScheduler.h"

#include <ctime>
#include <vector>

namespace {
    constexpr uint64_t CHECK_INTERVAL = 60;
}

TempbanScheduler::TempbanScheduler(dpp::cluster &bot, const Config &config, ModerationStore &store,
                                   ModerationLog &log)
    : mBot(bot), mConfig(config), mStore(store), mLog(log) {
}

void TempbanScheduler::start() {
    _expire();
    mBot.start_timer([this](dpp::timer) {
        _expire();
    }, CHECK_INTERVAL);
}

dpp::job TempbanScheduler::_expire() {
    const int64_t now = static_cast<int64_t>(std::time(nullptr));
    const std::vector<ModerationCase> expired = mStore.takeExpiredTempbans(now);
    for (const ModerationCase &tempban : expired) {
        mBot.set_audit_reason("Tempban #" + std::to_string(tempban.mId) + " expired");
        const dpp::confirmation_callback_t result = co_await mBot.co_guild_ban_delete(mConfig.mGuildId,
                                                                                      tempban.mUserId);
        if (result.is_error()) {
            mBot.log(dpp::ll_warning, "Could not lift tempban #" + std::to_string(tempban.mId) + ": " +
                                      result.get_error().human_readable);
            continue;
        }

        ModerationCase unban;
        unban.mType = CaseType::Unban;
        unban.mUserId = tempban.mUserId;
        unban.mModeratorId = mBot.me.id;
        unban.mReason = "Tempban #" + std::to_string(tempban.mId) + " expired";
        unban.mCreatedAt = now;
        unban.mId = mStore.addCase(unban);
        mLog.recordCase(unban, tempban.mUserId.str());
    }
}
