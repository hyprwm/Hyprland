#pragma once

#include "../../helpers/memory/Memory.hpp"

#include <expected>
#include <string>
#include <vector>

namespace Config::Supplementary {
    class CConfigFixer {
      public:
        CConfigFixer()  = default;
        ~CConfigFixer() = default;

        CConfigFixer(const CConfigFixer&) = delete;
        CConfigFixer(CConfigFixer&)       = delete;
        CConfigFixer(CConfigFixer&&)      = delete;

        // returns failed checks
        size_t validate(const std::vector<std::string>& paths);

        // returns zip path or error message
        std::expected<std::string, std::string> backupConfigs(const std::vector<std::string>& paths);

        // returns success
        bool fix(const std::vector<std::string>& paths);
    };

    const UP<CConfigFixer>& fixer();
};