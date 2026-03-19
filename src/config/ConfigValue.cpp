#include "ConfigValue.hpp"
#include "ConfigManager.hpp"

void local__configValuePopulate(void* const** p, const std::string& val) {
    const auto BIGP = Config::mgr()->getConfigValue(val);
    *p              = BIGP.dataptr;
}

std::type_index local__configValueTypeIdx(const std::string& val) {
    const auto BIGP = Config::mgr()->getConfigValue(val);
    return std::type_index(*BIGP.type);
}