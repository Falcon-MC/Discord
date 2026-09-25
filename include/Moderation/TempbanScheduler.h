#pragma once

#include "Config.h"
#include "Moderation/ModerationLog.h"
#include "Moderation/ModerationStore.h"

#include <dpp/dpp.h>

class TempbanScheduler {
public:
    TempbanScheduler(dpp::cluster &bot, const Config &config, ModerationStore &store, ModerationLog &log);

    void start();

private:
    dpp::cluster &mBot;
    const Config &mConfig;
    ModerationStore &mStore;
    ModerationLog &mLog;

    dpp::job _expire();
};
