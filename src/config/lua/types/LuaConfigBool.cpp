#include "LuaConfigBool.hpp"

using namespace Config;
using namespace Config::Lua;

CLuaConfigBool::CLuaConfigBool(Config::BOOL def) : m_data(def) {
    ;
}

SParseError CLuaConfigBool::parse(lua_State* s) {
    if (lua_isboolean(s, -1)) {
        m_data       = lua_toboolean(s, -1);
        m_bSetByUser = true;
        return {.errorCode = PARSE_ERROR_OK};
    }

    return {.errorCode = PARSE_ERROR_BAD_TYPE, .message = "boolean type requires a bool"};
}

const std::type_info* CLuaConfigBool::underlying() {
    return &typeid(decltype(m_data));
}

void const* CLuaConfigBool::data() {
    return &m_data;
}

std::string CLuaConfigBool::toString() {
    return m_data ? "1" : "0";
}
