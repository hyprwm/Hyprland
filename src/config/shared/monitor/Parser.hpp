#pragma once

#include "MonitorRule.hpp"

namespace Config {
    class CMonitorRuleParser {
      public:
        CMonitorRuleParser(const std::string& name);

        const std::string&         name();
        CMonitorRule&              rule();
        std::optional<std::string> getError();
        bool                       parseMode(const std::string& value);
        bool                       parsePosition(const std::string& value, bool isFirst = false);
        bool                       parseScale(const std::string& value);
        bool                       parseTransform(const std::string& value);
        bool                       parseBitdepth(const std::string& value);
        bool                       parseCM(const std::string& value);
        bool                       parseSDRBrightness(const std::string& value);
        bool                       parseSDRSaturation(const std::string& value);
        bool                       parseVRR(const std::string& value);
        bool                       parseICC(const std::string& value);

        void                       setDisabled();
        void                       setMirror(const std::string& value);
        bool                       setReserved(const Desktop::CReservedArea& value);

      private:
        CMonitorRule m_rule;
        std::string  m_error = "";
    };
};