#include <config/lua/LuaEventHandler.hpp>

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

    int refGlobalFunction(lua_State* L, const char* name) {
        lua_getglobal(L, name);
        EXPECT_TRUE(lua_isfunction(L, -1));
        return luaL_ref(L, LUA_REGISTRYINDEX);
    }

    int getGlobalInt(lua_State* L, const char* name) {
        lua_getglobal(L, name);
        const auto val = lua_tointeger(L, -1);
        lua_pop(L, 1);
        return val;
    }

    const char* getGlobalString(lua_State* L, const char* name) {
        lua_getglobal(L, name);
        const auto val = lua_tostring(L, -1);
        lua_pop(L, 1);
        return val;
    }
}

TEST(ConfigLuaEventHandler, knownEventsContainsExpectedEntries) {
    const auto& known = CLuaEventHandler::knownEvents();
    EXPECT_TRUE(known.contains("config.reloaded"));
    EXPECT_TRUE(known.contains("keybinds.submap"));
    EXPECT_TRUE(known.contains("window.open"));
}

TEST(ConfigLuaEventHandler, registerDispatchAndUnregister) {
    CLuaState        S;
    const auto       L = S.get();
    CLuaEventHandler handler(L);

    ASSERT_EQ(luaL_dostring(L, R"(
        count = 0
        lastSubmap = ""
        function onSubmap(name)
            count = count + 1
            lastSubmap = name
        end
    )"),
              LUA_OK);

    const int  ref    = refGlobalFunction(L, "onSubmap");
    const auto handle = handler.registerEvent("keybinds.submap", ref);
    ASSERT_TRUE(handle.has_value());

    Event::bus()->m_events.keybinds.submap.emit("main");
    EXPECT_EQ(getGlobalInt(L, "count"), 1);
    EXPECT_STREQ(getGlobalString(L, "lastSubmap"), "main");

    EXPECT_TRUE(handler.unregisterEvent(*handle));

    Event::bus()->m_events.keybinds.submap.emit("other");
    EXPECT_EQ(getGlobalInt(L, "count"), 1);
}

TEST(ConfigLuaEventHandler, registerUnknownEventReturnsNullopt) {
    CLuaState        S;
    const auto       L = S.get();
    CLuaEventHandler handler(L);

    ASSERT_EQ(luaL_dostring(L, "function cb() end"), LUA_OK);

    const int  ref    = refGlobalFunction(L, "cb");
    const auto handle = handler.registerEvent("totally.unknown", ref);

    EXPECT_FALSE(handle.has_value());
    luaL_unref(L, LUA_REGISTRYINDEX, ref);
}

TEST(ConfigLuaEventHandler, callbackLuaErrorDoesNotCrashDispatch) {
    CLuaState        S;
    const auto       L = S.get();
    CLuaEventHandler handler(L);

    ASSERT_EQ(luaL_dostring(L, "function broken() error('boom') end"), LUA_OK);

    const int  ref = refGlobalFunction(L, "broken");
    const auto h   = handler.registerEvent("config.reloaded", ref);
    ASSERT_TRUE(h.has_value());

    Event::bus()->m_events.config.reloaded.emit();
    SUCCEED();
}

TEST(ConfigLuaEventHandler, addCustomEventAndDispatchToLua) {
    CLuaState        S;
    const auto       L = S.get();
    CLuaEventHandler handler(L);

    ASSERT_EQ(luaL_dostring(L, R"(
        count = 0
        str = ""
        function onMyEvent(a, b)
            count = a - 25
            str = b
        end
    )"),
              LUA_OK);

    const auto e =
        makeShared<Event::CEventBus::CCustomEvent>("plugin.my_event", std::vector{Event::CEventBus::CCustomEvent::TYPE_INT, Event::CEventBus::CCustomEvent::TYPE_STRING});
    EXPECT_TRUE(Event::bus()->addPluginEvent(e));
    EXPECT_TRUE(CLuaEventHandler::knownEvents().contains("plugin.my_event"));

    const int  ref    = refGlobalFunction(L, "onMyEvent");
    const auto handle = handler.registerEvent("plugin.my_event", ref);
    ASSERT_TRUE(handle.has_value());

    EXPECT_TRUE(e->emit({67, std::string("hello from plugin")}));
    EXPECT_EQ(getGlobalInt(L, "count"), 42);
    EXPECT_STREQ(getGlobalString(L, "str"), "hello from plugin");
}

TEST(ConfigLuaEventHandler, removingPluginEventUnregistersLuaEvent) {
    CLuaState        S;
    const auto       L = S.get();
    CLuaEventHandler handler(L);

    const auto       e = makeShared<Event::CEventBus::CCustomEvent>("plugin.to_be_removed", std::vector<Event::CEventBus::CCustomEvent::eType>{});
    EXPECT_TRUE(Event::bus()->addPluginEvent(e));
    EXPECT_TRUE(CLuaEventHandler::knownEvents().contains("plugin.to_be_removed"));

    EXPECT_TRUE(Event::bus()->removePluginEvent(e->m_name));
    EXPECT_FALSE(CLuaEventHandler::knownEvents().contains("plugin.to_be_removed"));
}

TEST(ConfigLuaEventHandler, addCustomEventDuplicateFails) {
    CLuaState        S;
    const auto       L = S.get();
    CLuaEventHandler handler(L);

    const auto       ev = makeShared<Event::CEventBus::CCustomEvent>("plugin.dup", std::vector<Event::CEventBus::CCustomEvent::eType>{});

    EXPECT_TRUE(handler.addCustomEvent(ev));
    EXPECT_FALSE(handler.addCustomEvent(ev));
}

TEST(ConfigLuaEventHandler, removeNonexistentCustomEventFails) {
    CLuaState        S;
    const auto       L = S.get();
    CLuaEventHandler handler(L);

    EXPECT_FALSE(handler.removeCustomEvent("plugin.nonexistent"));
}
