#pragma once

#include <dpp/dpp.h>

#include <functional>

std::function<void(const dpp::log_t &)> makeConsoleLogger();
