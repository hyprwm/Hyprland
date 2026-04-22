#include <config/lua/LuaEventHandler.hpp>

#include <config/lua/objects/LuaEventSubscription.hpp>
#include <config/lua/objects/LuaKeybind.hpp>

#include <managers/KeybindManager.hpp>
#include <event/EventBus.hpp>

#include <gtest/gtest.h>

extern "C" {
#include <lualib.h>
#include <lauxlib.h>
}

using namespace Config::Lua;

namespace {
    class CLuaState {
      public:
        CLuaState() : m_lua(luaL_newstate()) {
            luaL_openlibs(m_lua);
        }

        ~CLuaState() {
            if (m_lua)
                lua_close(m_lua);
        }

        lua_State* get() const {
            return m_lua;
        }

      private:
        lua_State* m_lua = nullptr;
    };

    int getGlobalInt(lua_State* L, const char* name) {
        lua_getglobal(L, name);
        const int v = sc<int>(lua_tointeger(L, -1));
        lua_pop(L, 1);
        return v;
    }

    bool getGlobalBool(lua_State* L, const char* name) {
        lua_getglobal(L, name);
        const bool v = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
        return v;
    }
}

TEST(ConfigLuaObjects, keybindCanToggleEnabledFromLua) {
    CLuaState  S;
    const auto L = S.get();

    Objects::CLuaKeybind{}.setup(L);

    auto keybind     = makeShared<SKeybind>();
    keybind->enabled = true;

    Objects::CLuaKeybind::push(L, keybind);
    lua_setglobal(L, "kb");

    ASSERT_EQ(luaL_dostring(L, R"(
        assert(kb:is_enabled() == true)
        kb:set_enabled(false)
        assert(kb:is_enabled() == false)
    )"),
              LUA_OK);

    EXPECT_FALSE(keybind->enabled);
}

TEST(ConfigLuaObjects, keybindExposesMetadataAndRemoveMethods) {
    CLuaState  S;
    const auto L = S.get();

    Objects::CLuaKeybind{}.setup(L);

    auto keybind             = makeShared<SKeybind>();
    keybind->enabled         = true;
    keybind->description     = "Close active window";
    keybind->hasDescription  = true;
    keybind->displayKey      = "SUPER + Q";
    keybind->submap.name     = "default";
    keybind->handler         = "exec";
    keybind->arg             = "kitty";
    keybind->modmask         = HL_MODIFIER_META;
    keybind->key             = "Q";
    keybind->keycode         = 24;
    keybind->repeat          = true;
    keybind->locked          = true;
    keybind->release         = false;
    keybind->nonConsuming    = true;
    keybind->transparent     = false;
    keybind->ignoreMods      = false;
    keybind->longPress       = false;
    keybind->dontInhibit     = true;
    keybind->click           = false;
    keybind->drag            = false;
    keybind->submapUniversal = false;
    keybind->deviceInclusive = true;
    keybind->devices         = {"kbd-a", "kbd-b"};

    Objects::CLuaKeybind::push(L, keybind);
    lua_setglobal(L, "kb");

    ASSERT_EQ(luaL_dostring(L, R"(
        assert(kb.enabled == true)
        assert(kb.description == "Close active window")
        assert(kb.display_key == "SUPER + Q")
        assert(kb.submap == "default")
        assert(kb.handler == "exec")
        assert(kb.arg == "kitty")
        assert(kb.modmask ~= nil)
        assert(kb.key == "Q")
        assert(kb.keycode == 24)
        assert(kb.repeating == true)
        assert(kb.locked == true)
        assert(kb.non_consuming == true)
        assert(kb.dont_inhibit == true)
        assert(type(kb.devices) == "table")

        kb:remove()
        kb:unbind()
    )"),
              LUA_OK);
}

TEST(ConfigLuaObjects, objectsAreReadOnlyFromLua) {
    CLuaState  S;
    const auto L = S.get();

    Objects::CLuaKeybind{}.setup(L);

    auto keybind = makeShared<SKeybind>();
    Objects::CLuaKeybind::push(L, keybind);
    lua_setglobal(L, "kb");

    EXPECT_NE(luaL_dostring(L, "kb.foo = 1"), LUA_OK);
    ASSERT_TRUE(lua_isstring(L, -1));
    EXPECT_NE(std::string(lua_tostring(L, -1)).find("read-only"), std::string::npos);
    lua_pop(L, 1);
}

TEST(ConfigLuaObjects, keybindSupportsEqAndToString) {
    CLuaState  S;
    const auto L = S.get();

    Objects::CLuaKeybind{}.setup(L);

    auto keybindA = makeShared<SKeybind>();
    auto keybindB = makeShared<SKeybind>();

    Objects::CLuaKeybind::push(L, keybindA);
    lua_setglobal(L, "kb1");

    Objects::CLuaKeybind::push(L, keybindA);
    lua_setglobal(L, "kb2");

    Objects::CLuaKeybind::push(L, keybindB);
    lua_setglobal(L, "kb3");

    ASSERT_EQ(luaL_dostring(L, R"(
        eq12  = (kb1 == kb2)
        neq13 = (kb1 ~= kb3)

        local s = tostring(kb1)
        hasPrefix = type(s) == "string" and string.sub(s, 1, 11) == "HL.Keybind("
    )"),
              LUA_OK);

    EXPECT_TRUE(getGlobalBool(L, "eq12"));
    EXPECT_TRUE(getGlobalBool(L, "neq13"));
    EXPECT_TRUE(getGlobalBool(L, "hasPrefix"));
}

TEST(ConfigLuaObjects, eventSubscriptionRemoveAndIsActive) {
    CLuaState  S;
    const auto L = S.get();

    Objects::CLuaEventSubscription{}.setup(L);
    CLuaEventHandler handler(L);

    ASSERT_EQ(luaL_dostring(L, R"(
        count = 0
        function onReload()
            count = count + 1
        end
    )"),
              LUA_OK);

    lua_getglobal(L, "onReload");
    ASSERT_TRUE(lua_isfunction(L, -1));
    const int  ref = luaL_ref(L, LUA_REGISTRYINDEX);

    const auto handle = handler.registerEvent("config.reloaded", ref);
    ASSERT_TRUE(handle.has_value());

    Objects::CLuaEventSubscription::push(L, &handler, *handle);
    lua_setglobal(L, "sub");

    Event::bus()->m_events.config.reloaded.emit();
    EXPECT_EQ(getGlobalInt(L, "count"), 1);

    ASSERT_EQ(luaL_dostring(L, R"(
        assert(sub:is_active() == true)
        sub:remove()
        assert(sub:is_active() == false)
        sub:remove()
    )"),
              LUA_OK);

    Event::bus()->m_events.config.reloaded.emit();
    EXPECT_EQ(getGlobalInt(L, "count"), 1);
}

TEST(ConfigLuaObjects, eventSubscriptionSupportsEqAndToString) {
    CLuaState  S;
    const auto L = S.get();

    Objects::CLuaEventSubscription{}.setup(L);
    CLuaEventHandler handler(L);

    ASSERT_EQ(luaL_dostring(L, R"(
        function cbA() end
        function cbB() end
    )"),
              LUA_OK);

    lua_getglobal(L, "cbA");
    ASSERT_TRUE(lua_isfunction(L, -1));
    const int refA = luaL_ref(L, LUA_REGISTRYINDEX);

    lua_getglobal(L, "cbB");
    ASSERT_TRUE(lua_isfunction(L, -1));
    const int  refB = luaL_ref(L, LUA_REGISTRYINDEX);

    const auto handleA = handler.registerEvent("config.reloaded", refA);
    const auto handleB = handler.registerEvent("config.reloaded", refB);
    ASSERT_TRUE(handleA.has_value());
    ASSERT_TRUE(handleB.has_value());

    Objects::CLuaEventSubscription::push(L, &handler, *handleA);
    lua_setglobal(L, "sub1");

    Objects::CLuaEventSubscription::push(L, &handler, *handleA);
    lua_setglobal(L, "sub2");

    Objects::CLuaEventSubscription::push(L, &handler, *handleB);
    lua_setglobal(L, "sub3");

    ASSERT_EQ(luaL_dostring(L, R"(
        eq12  = (sub1 == sub2)
        neq13 = (sub1 ~= sub3)

        local s = tostring(sub1)
        hasPrefix = type(s) == "string" and string.sub(s, 1, 21) == "HL.EventSubscription("
    )"),
              LUA_OK);

    EXPECT_TRUE(getGlobalBool(L, "eq12"));
    EXPECT_TRUE(getGlobalBool(L, "neq13"));
    EXPECT_TRUE(getGlobalBool(L, "hasPrefix"));
}
