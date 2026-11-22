#include <config/lua/decoration/LuaDecorationProvider.hpp>

#include <gtest/gtest.h>

#include <format>
#include <string>
#include <utility>
#include <vector>

extern "C" {
#include <lualib.h>
#include <lauxlib.h>
}

using namespace Config::Lua::Decorations;

namespace {
    class CLuaState {
      public:
        CLuaState() : m_lua(luaL_newstate()) {
            luaL_openlibs(m_lua);
        }

        ~CLuaState() {
            lua_close(m_lua);
        }

        std::expected<SToolkitDecorationSpec, std::string> parse(const std::string& table) const {
            EXPECT_EQ(luaL_dostring(m_lua, std::format("return {}", table).c_str()), LUA_OK);

            auto spec = parseSpec(m_lua, -1);
            lua_pop(m_lua, 1);

            EXPECT_EQ(lua_gettop(m_lua), 0);
            return spec;
        }

      private:
        lua_State* m_lua = nullptr;
    };
}

TEST(ConfigLuaDecorationSpec, parsesAllFields) {
    CLuaState  lua;
    const auto SPEC = lua.parse(R"({ edge = "left", size = 24, priority = 3, reserved = false, layer = "overlay", input = false, create = function() end })");

    ASSERT_TRUE(SPEC.has_value()) << SPEC.error();
    EXPECT_EQ(SPEC->edge, DECORATION_EDGE_LEFT);
    EXPECT_EQ(SPEC->size, 24);
    EXPECT_EQ(SPEC->priority, 3u);
    EXPECT_FALSE(SPEC->reserved);
    EXPECT_EQ(SPEC->layer, DECORATION_LAYER_OVERLAY);
    EXPECT_FALSE(SPEC->input);
}

TEST(ConfigLuaDecorationSpec, appliesDefaults) {
    CLuaState  lua;
    const auto SPEC = lua.parse(R"({ edge = "top", size = 28, create = function() end })");

    ASSERT_TRUE(SPEC.has_value()) << SPEC.error();
    EXPECT_EQ(SPEC->edge, DECORATION_EDGE_TOP);
    EXPECT_EQ(SPEC->priority, SToolkitDecorationSpec{}.priority);
    EXPECT_TRUE(SPEC->reserved);
    EXPECT_EQ(SPEC->layer, DECORATION_LAYER_OVER);
    EXPECT_TRUE(SPEC->input);
}

TEST(ConfigLuaDecorationSpec, rejectsInvalidSpecs) {
    CLuaState                                              lua;

    const std::vector<std::pair<std::string, std::string>> CASES = {
        {R"({ size = 28, create = function() end })", "missing field \"edge\""},
        {R"({ edge = "middle", size = 28, create = function() end })", "field \"edge\""},
        {R"({ edge = "top", create = function() end })", "missing field \"size\""},
        {R"({ edge = "top", size = 0, create = function() end })", "field \"size\""},
        {R"({ edge = "top", size = 1.5, create = function() end })", "field \"size\""},
        {R"({ edge = "top", size = 28, priority = -1, create = function() end })", "field \"priority\""},
        {R"({ edge = "top", size = 28, reserved = "yes", create = function() end })", "field \"reserved\""},
        {R"({ edge = "top", size = 28, layer = "above", create = function() end })", "field \"layer\""},
        {R"({ edge = "top", size = 28 })", "field \"create\""},
        {R"({ edge = "top", sise = 28, create = function() end })", "unknown field \"sise\""},
        {R"({ edge = "top", size = 28, create = function() end, 42 })", "unexpected number key"},
    };

    for (const auto& [table, error] : CASES) {
        const auto SPEC = lua.parse(table);
        ASSERT_FALSE(SPEC.has_value()) << table;
        EXPECT_TRUE(SPEC.error().contains(error)) << table << " -> " << SPEC.error();
    }
}
