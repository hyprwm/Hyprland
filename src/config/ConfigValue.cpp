#include "ConfigValue.hpp"
#include "ConfigManager.hpp"

void local__configValuePopulate(void* const** p, void* const** hlangp, std::type_index* ti, const std::string& val) {
    const auto BIGP = Config::mgr()->getConfigValue(val);

    RASSERT(BIGP.dataptr, "Something went really fucking wrong with config values");

    *ti = std::type_index(*BIGP.type);

    if (std::type_index(*BIGP.type) == typeid(void*) || std::type_index(*BIGP.type) == typeid(const char*)) {
        // this is a special, cursed case. ew.
        *hlangp = BIGP.dataptr;
    } else
        *p = BIGP.dataptr;
}

std::type_index local__configValueTypeIdx(const std::string& val) {
    const auto BIGP = Config::mgr()->getConfigValue(val);
    return std::type_index(*BIGP.type);
}