#pragma once

#include <hyprutils/cli/Logger.hpp>

#include "../../helpers/memory/Memory.hpp"
#include "../../helpers/env/Env.hpp"

namespace Log {
    class CLogger {
      public:
        CLogger();
        ~CLogger() = default;

        void initIS(const std::string_view& IS);
        void initCallbacks();

        void log(Hyprutils::CLI::eLogLevel level, const std::string_view& str);

        template <typename... Args>
        //NOLINTNEXTLINE
        void log(Hyprutils::CLI::eLogLevel level, std::format_string<Args...> fmt, Args&&... args) {
            static bool TRACE = Env::isTrace();

            if (!m_logsEnabled)
                return;

            if (level == Hyprutils::CLI::LOG_TRACE && !TRACE)
                return;

            std::string logMsg = "";

            // no need for try {} catch {} because std::format_string<Args...> ensures that vformat never throw std::format_error
            // because
            // 1. any faulty format specifier that sucks will cause a compilation error.
            // 2. and `std::bad_alloc` is catastrophic, (Almost any operation in stdlib could throw this.)
            // 3. this is actually what std::format in stdlib does
            logMsg += std::vformat(fmt.get(), std::make_format_args(args...));

            log(level, logMsg);
        }

        const std::string&       rolling();
        Hyprutils::CLI::CLogger& hu();

      private:
        void                    recheckCfg();

        Hyprutils::CLI::CLogger m_logger;
        bool                    m_logsEnabled = true;
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
