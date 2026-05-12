#include "LuaConfigValue.hpp"

#include "../../../macros.hpp"

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

Config::INTEGER ILuaConfigValue::asInt() {
    RASSERT(false, "Lua config value cannot be read as int");
    return 0;
}

Config::FLOAT ILuaConfigValue::asFloat() {
    RASSERT(false, "Lua config value cannot be read as float");
    return 0.F;
}

Config::VEC2 ILuaConfigValue::asVec2() {
    RASSERT(false, "Lua config value cannot be read as vec2");
    return {};
}

Config::STRING ILuaConfigValue::asString() {
    RASSERT(false, "Lua config value cannot be read as string");
    return {};
}
