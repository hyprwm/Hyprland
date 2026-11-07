#include "ConfigValue.hpp"
#include "ConfigManager.hpp"

#include <algorithm>

void local__configValuePopulate(void* const** p, const bool** setByUser, std::type_index* ti, const std::string& val) {
    const auto BIGP = Config::mgr()->getConfigValue(val);

    RASSERT(BIGP.dataptr, "Something went really fucking wrong with config values");

    *ti        = std::type_index(*BIGP.type);
    *setByUser = BIGP.setByUser;

    *p = BIGP.dataptr;
}

std::type_index local__configValueTypeIdx(const std::string& val) {
    const auto BIGP = Config::mgr()->getConfigValue(val);
    return std::type_index(*BIGP.type);
}

CConfigValueBase::CConfigValueBase() {
    registry().emplace_back(this);
}

CConfigValueBase::~CConfigValueBase() {
    std::erase(registry(), this);
}

void CConfigValueBase::populateFromName() {
    m_p         = nullptr;
    m_typeIndex = typeid(void);
    if (!m_valueName.empty())
        local__configValuePopulate(&m_p, &m_setByUser, &m_typeIndex, m_valueName);
}

void CConfigValueBase::bindInternal(const std::string& val) {
    m_valueName = val;
    populateFromName();
}

std::vector<CConfigValueBase*>& CConfigValueBase::registry() {
    static std::vector<CConfigValueBase*> r;
    return r;
}

void CConfigValueBase::flushCaches() {
    for (const auto& v : registry()) {
        v->bindInternal(v->m_valueName);
    }
}
