#include "LuaConfigValue.hpp"

using namespace Config;
using namespace Config::Lua;

void ILuaConfigValue::resetSetByUser() {
    m_bSetByUser = false;
}

bool ILuaConfigValue::setByUser() {
    return m_bSetByUser;
}

void ILuaConfigValue::setRefreshBits(Supplementary::PropRefreshBits bits) {
    m_refreshBits = bits;
}

Supplementary::PropRefreshBits ILuaConfigValue::refreshBits() const {
    return m_refreshBits;
}
