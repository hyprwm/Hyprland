#pragma once

#include <string>
#include <cstdint>
#include <expected>

extern "C" {
#include <lua.h>
}

namespace Config::Lua::Bindings::Check {
    std::expected<std::string, std::string> string(lua_State* L, int idx);
    std::expected<int64_t, std::string>     integer(lua_State* L, int idx);
    std::expected<double, std::string>      number(lua_State* L, int idx);
    std::expected<bool, std::string>        boolean(lua_State* L, int idx);
};
