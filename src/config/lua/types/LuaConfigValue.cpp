#include "LuaConfigValue.hpp"

using namespace Config;
using namespace Config::Lua;

void ILuaConfigValue::resetSetByUser() {
    m_bSetByUser = false;
}

bool ILuaConfigValue::setByUser() {
    return m_bSetByUser;
}
