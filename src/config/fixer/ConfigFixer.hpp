#pragma once

#include "../../helpers/memory/Memory.hpp"

namespace Config::Supplementary {
    class CConfigFixer {
      public:
        CConfigFixer()  = default;
        ~CConfigFixer() = default;

        CConfigFixer(const CConfigFixer&) = delete;
        CConfigFixer(CConfigFixer&)       = delete;
        CConfigFixer(CConfigFixer&&)      = delete;

        void validate(const std::vector<std::string>& paths);
        void fix(const std::vector<std::string>& paths);
    };

    const UP<CConfigFixer>& fixer();
};