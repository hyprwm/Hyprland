#pragma once

#include <hyprutils/cli/Logger.hpp>

#include "Memory.hpp"

// we do this to add a from start-hyprland to the logs
inline UP<Hyprutils::CLI::CLogger>           g_loggerMain = makeUnique<Hyprutils::CLI::CLogger>();
inline UP<Hyprutils::CLI::CLoggerConnection> g_logger;
