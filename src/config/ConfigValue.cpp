#include "ConfigValue.hpp"
#include "ConfigManager.hpp"
#include "../macros.hpp"

void local__configValuePopulate(void* const** p, const std::string& val) {
    const auto PVHYPRLANG = g_pConfigManager->getHyprlangConfigValuePtr(val);

    *p = PVHYPRLANG->getDataStaticPtr();
}

std::type_index local__configValueTypeIdx(const std::string& val) {
    const auto PVHYPRLANG = g_pConfigManager->getHyprlangConfigValuePtr(val);
    const auto ANY        = PVHYPRLANG->getValue();
    return std::type_index(ANY.type());
}