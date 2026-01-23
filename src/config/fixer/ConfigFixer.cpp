#include "ConfigFixer.hpp"

using namespace Config::Supplementary;
using namespace Config;

const UP<CConfigFixer>& Supplementary::fixer() {
    static UP<CConfigFixer> fixer = makeUnique<CConfigFixer>();
    return fixer;
}

