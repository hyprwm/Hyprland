#pragma once

#include <hyprutils/cli/Logger.hpp>
#include <source_location>
#include <utility>

#include "../../helpers/memory/Memory.hpp"
#include "../../helpers/env/Env.hpp"

#define LOG(level, fmt, ...)                                                                                                                                                       \
    do {                                                                                                                                                                           \
        Log::logger->log(level, Log::logFnName(), fmt __VA_OPT__(, ) __VA_ARGS__);                                                                                                 \
    } while (0)

namespace Log {

    consteval std::string_view logFnName(std::source_location loc = std::source_location::current()) {
        std::string_view name = loc.function_name();

        // this usually returns something like:
        //    void Fuck::fucker(float)
        // and we basically only want the "Fuck::fucker"

        if (const auto P = name.find(' '); P != std::string::npos)
            name = name.substr(P + 1);
        if (const auto P = name.find('('); P != std::string::npos)
            name = name.substr(0, P);

        return name;
    }

    class CLogger {
      public:
        CLogger();
        ~CLogger() = default;

        void initIS(const std::string_view& IS);
        void initCallbacks();

        void log(Hyprutils::CLI::eLogLevel level, const std::string_view& str);
        void log(Hyprutils::CLI::eLogLevel level, const std::string_view loc, const std::string_view str);

        template <typename... Args>
        // NOLINTNEXTLINE
        void log(Hyprutils::CLI::eLogLevel level, const std::string_view loc, std::format_string<Args...> fmt, Args&&... args) {
            if (!m_logsEnabled)
                return;

            if (level == Hyprutils::CLI::LOG_TRACE && !m_isTrace)
                return;

            std::string logMsg = std::format("[{}] ", loc);
            logMsg += std::format(fmt, std::forward<Args>(args)...);

            log(level, logMsg);
        }

        const std::string&       rolling();
        Hyprutils::CLI::CLogger& hu();

      private:
        void                    recheckCfg();

        Hyprutils::CLI::CLogger m_logger;
        bool                    m_logsEnabled = true;
        bool                    m_isTrace     = false;
    };

    inline UP<CLogger> logger = makeUnique<CLogger>();

    //
    inline constexpr const Hyprutils::CLI::eLogLevel DEBUG = Hyprutils::CLI::LOG_DEBUG;
    inline constexpr const Hyprutils::CLI::eLogLevel WARN  = Hyprutils::CLI::LOG_WARN;
    inline constexpr const Hyprutils::CLI::eLogLevel ERR   = Hyprutils::CLI::LOG_ERR;
    inline constexpr const Hyprutils::CLI::eLogLevel CRIT  = Hyprutils::CLI::LOG_CRIT;
    inline constexpr const Hyprutils::CLI::eLogLevel INFO  = Hyprutils::CLI::LOG_DEBUG;
    inline constexpr const Hyprutils::CLI::eLogLevel TRACE = Hyprutils::CLI::LOG_TRACE;
};
