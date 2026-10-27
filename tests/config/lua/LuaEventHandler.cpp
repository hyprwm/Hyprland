#include <config/lua/LuaEventHandler.hpp>
#include <config/lua/LuaCaller.hpp>

#include <event/EventBus.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

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

    CLuaRef refGlobalFunction(lua_State* L, const char* name) {
        lua_getglobal(L, name);
        EXPECT_TRUE(lua_isfunction(L, -1));
        CLuaRef ref{makeShared<SLuaStateLifetime>(L), L, -1};
        lua_pop(L, 1);
        return ref;
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
    CLuaCaller       caller(L, [](std::string&&) {});
    CLuaEventHandler handler(caller);

    ASSERT_EQ(luaL_dostring(L, R"(
        count = 0
        lastSubmap = ""
        function onSubmap(name)
            count = count + 1
            lastSubmap = name
        end
    )"),
              LUA_OK);

    auto       ref    = refGlobalFunction(L, "onSubmap");
    const auto handle = handler.registerEvent("keybinds.submap", std::move(ref));
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
    CLuaCaller       caller(L, [](std::string&&) {});
    CLuaEventHandler handler(caller);

    ASSERT_EQ(luaL_dostring(L, "function cb() end"), LUA_OK);

    auto       ref    = refGlobalFunction(L, "cb");
    const auto handle = handler.registerEvent("totally.unknown", std::move(ref));

    EXPECT_FALSE(handle.has_value());
}

TEST(ConfigLuaEventHandler, callbackErrorIsReportedOnce) {
    CLuaState                S;
    const auto               L = S.get();
    std::vector<std::string> errors;
    CLuaCaller               caller(L, [&errors](std::string&& e) { errors.emplace_back(std::move(e)); });
    CLuaEventHandler         handler(caller);

    ASSERT_EQ(luaL_dostring(L, "function broken() error('boom', 0) end"), LUA_OK);

    auto       ref = refGlobalFunction(L, "broken");
    const auto h   = handler.registerEvent("config.reloaded", std::move(ref));
    ASSERT_TRUE(h.has_value());

    Event::bus()->m_events.config.reloaded.emit();
    Event::bus()->m_events.config.reloaded.emit();

    ASSERT_EQ(errors.size(), 1);
    EXPECT_EQ(errors.front(), "hl.on(\"config.reloaded\") callback: boom");
}

// one function registered for two events, where the first event synchronously fires the second
static int emitReload(lua_State* L) {
    Event::bus()->m_events.config.reloaded.emit();
    return 0;
}

TEST(ConfigLuaEventHandler, sharedHandlerIsNotReenteredByAnotherEvent) {
    CLuaState        S;
    const auto       L = S.get();
    CLuaCaller       caller(L, [](std::string&&) {});
    CLuaEventHandler handler(caller);
    lua_register(L, "emit_reload", emitReload);

    ASSERT_EQ(luaL_dostring(L, R"(
        count = 0
        function shared()
            count = count + 1
            emit_reload()
        end
        function other() others = (others or 0) + 1 end
    )"),
              LUA_OK);

    ASSERT_TRUE(handler.registerEvent("keybinds.submap", refGlobalFunction(L, "shared")).has_value());
    ASSERT_TRUE(handler.registerEvent("config.reloaded", refGlobalFunction(L, "shared")).has_value());
    ASSERT_TRUE(handler.registerEvent("config.reloaded", refGlobalFunction(L, "other")).has_value());

    Event::bus()->m_events.keybinds.submap.emit("main");

    // the nested config.reloaded skips the already-running function but still reaches other handlers
    EXPECT_EQ(getGlobalInt(L, "count"), 1);
    EXPECT_EQ(getGlobalInt(L, "others"), 1);
}
