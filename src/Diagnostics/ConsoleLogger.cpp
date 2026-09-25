#include "Diagnostics/ConsoleLogger.h"

#include <iostream>
#include <mutex>

std::function<void(const dpp::log_t &)> makeConsoleLogger() {
    return [](const dpp::log_t &event) {
        if (event.severity < dpp::ll_info)
            return;

        static std::mutex mutex;
        std::lock_guard<std::mutex> lock(mutex);
        std::cout << "[" << dpp::utility::current_date_time() << "] " << dpp::utility::loglevel(event.severity)
                  << ": " << event.message << std::endl;
    };
}
