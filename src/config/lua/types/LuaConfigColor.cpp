#include "LuaConfigColor.hpp"

#include <expected>
#include <format>

#include "../../../helpers/Color.hpp"
#include "../../../helpers/MiscFunctions.hpp"

using namespace Config;
using namespace Config::Lua;

static std::expected<CHyprColor, std::string> parseColorString(const std::string& str) {
    auto result = configStringToInt(str);
    if (!result)
        return std::unexpected(std::format("invalid color \"{}\"", str));
    return CHyprColor(sc<uint64_t>(*result));
}

CLuaConfigColor::CLuaConfigColor(Config::INTEGER def) : m_default(def), m_data(def) {
    ;
}

SParseError CLuaConfigColor::parse(lua_State* s) {
    if (lua_isstring(s, -1)) {
        auto data = lua_tostring(s, -1);

        auto col = parseColorString(data);

        if (!col)
            return {.errorCode = PARSE_ERROR_BAD_VALUE, .message = col.error()};

        m_data       = col->getAsHex();
        m_bSetByUser = true;
        return {.errorCode = PARSE_ERROR_OK};
    }

    return {.errorCode = PARSE_ERROR_BAD_TYPE, .message = "color type requires a color string"};
}

const std::type_info* CLuaConfigColor::underlying() {
    return &typeid(decltype(m_data));
}

void const* CLuaConfigColor::data() {
    return &m_data;
}

std::string CLuaConfigColor::toString() {
    return std::format("0x{:08X}", (uint32_t)m_data);
}

void CLuaConfigColor::push(lua_State* s) {
    const auto col = toString();
    lua_pushstring(s, col.c_str());
}

const Config::INTEGER& CLuaConfigColor::parsed() {
    return m_data;
}

void CLuaConfigColor::reset() {
    m_data = m_default;
}
