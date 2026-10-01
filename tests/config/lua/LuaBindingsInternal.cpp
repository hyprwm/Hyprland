#include <config/lua/bindings/LuaBindingsInternal.hpp>
#include <config/lua/ConfigManager.hpp>
#include <config/shared/inotify/ConfigWatcher.hpp>

#include <Compositor.hpp>

#include <config/lua/types/LuaConfigInt.hpp>
#include <config/values/types/IntValue.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>

extern "C" {
#include <lualib.h>
#include <lauxlib.h>
}

using namespace Config::Lua;
using namespace Config::Lua::Bindings;

namespace Config::Lua {
    class CConfigManagerPluginLuaTestAccessor {
      public:
        static void initializeLuaState(CConfigManager& mgr, lua_State* L) {
            mgr.m_lua = L;
            lua_pushlightuserdata(L, &mgr);
            lua_setfield(L, LUA_REGISTRYINDEX, "hl_lua_manager");
        }

        static void initializeOwnedLuaState(CConfigManager& mgr, const std::filesystem::path& mainConfigPath) {
            mgr.m_mainConfigPath = mainConfigPath.string();
            mgr.m_configPaths.clear();
            mgr.m_configPaths.emplace_back(mgr.m_mainConfigPath);
            mgr.reinitLuaState();
        }

        static lua_State* luaState(CConfigManager& mgr) {
            return mgr.m_lua;
        }
    };
}

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

    int testPluginFn(lua_State* L) {
        lua_pushstring(L, "pong");
        return 1;
    }

    class CTempDir {
      public:
        CTempDir() {
            const auto NOW = std::chrono::steady_clock::now().time_since_epoch().count();
            m_path         = std::filesystem::temp_directory_path() / std::format("hyprland-lua-require-{}", NOW);
            std::filesystem::create_directories(m_path);
        }

        ~CTempDir() {
            std::error_code ec;
            std::filesystem::remove_all(m_path, ec);
        }

        const std::filesystem::path& path() const {
            return m_path;
        }

      private:
        std::filesystem::path m_path;
    };

    class CScopedCompositor {
      public:
        CScopedCompositor() : m_prevCompositor(std::move(g_pCompositor)), m_prevKeybindManager(std::move(Keybinds::mgr())) {
            g_pCompositor   = makeUnique<CCompositor>(true);
            Keybinds::mgr() = makeUnique<Keybinds::CKeybindManager>();
        }

        ~CScopedCompositor() {
            Keybinds::mgr() = std::move(m_prevKeybindManager);
            g_pCompositor   = std::move(m_prevCompositor);
        }

      private:
        UP<CCompositor>               m_prevCompositor;
        UP<Keybinds::CKeybindManager> m_prevKeybindManager;
    };

    std::string luaString(const std::string& value) {
        std::string out = "\"";
        for (const auto& c : value) {
            if (c == '\\' || c == '"')
                out += '\\';
            out += c;
        }
        out += '"';
        return out;
    }

    void writeFile(const std::filesystem::path& path, const std::string& content) {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream file(path);
        file << content;
    }

    std::string normalizedPath(const std::filesystem::path& path) {
        return path.lexically_normal().string();
    }

    std::string packagePath(lua_State* L) {
        lua_getglobal(L, "package");
        lua_getfield(L, -1, "path");

        std::string path;
        if (const auto* value = lua_tostring(L, -1); value)
            path = value;

        lua_pop(L, 2);
        return path;
    }

    void expectTracked(CConfigManager& mgr, const std::filesystem::path& path) {
        const auto& paths = mgr.getConfigPaths();
        EXPECT_NE(std::ranges::find(paths, normalizedPath(path)), paths.end());
    }
}

class CConfigLuaDispatchers : public testing::Test {
  protected:
    void SetUp() override {
        m_previousWatcher = std::move(Config::watcher());
        Config::watcher() = makeUnique<Config::CConfigWatcher>();
        m_previousManager = std::move(Config::mgr());
        Config::mgr()     = makeUnique<CConfigManager>();
        m_manager         = Config::Lua::mgr();
        CConfigManagerPluginLuaTestAccessor::initializeOwnedLuaState(*m_manager, m_tmp.path() / "hyprland.lua");
        m_lua = CConfigManagerPluginLuaTestAccessor::luaState(*m_manager);
    }

    void TearDown() override {
        Keybinds::mgr()->clearBinds();
        Config::mgr()     = std::move(m_previousManager);
        Config::watcher() = std::move(m_previousWatcher);
    }

    WP<CConfigManager> m_manager;
    lua_State*         m_lua = nullptr;

  private:
    CScopedCompositor          m_compositor;
    CTempDir                   m_tmp;
    UP<Config::IConfigManager> m_previousManager;
    UP<Config::CConfigWatcher> m_previousWatcher;
};

TEST_F(CConfigLuaDispatchers, rejectsFactoriesInBindAndDispatch) {
    for (const auto* factory : {"hl.dsp.exec_raw", "hl.dsp.no_op", "hl.dsp.window.close"}) {
        SCOPED_TRACE(factory);
        const auto aliasResult = m_manager->eval(std::format("factory = {}", factory));
        ASSERT_FALSE(aliasResult.has_value()) << aliasResult.value_or("");

        for (const auto* value : {factory, "factory"}) {
            for (const auto* api : {"hl.bind", "hl.dispatch"}) {
                SCOPED_TRACE(std::format("{}({})", api, value));
                const auto code   = std::string_view{api} == "hl.bind" ? std::format("hl.bind('SUPER + Q', {})", value) : std::format("hl.dispatch({})", value);
                const auto result = m_manager->eval(code);
                ASSERT_TRUE(result.has_value());
                EXPECT_NE(result->find(std::format("{}: dispatcher factory supplied", api)), std::string::npos);
                EXPECT_NE(result->find("missing parentheses?"), std::string::npos);
                EXPECT_TRUE(Keybinds::mgr()->registry().empty());
                EXPECT_EQ(lua_gettop(m_lua), 0);
            }
        }
    }
}

TEST_F(CConfigLuaDispatchers, acceptsDispatchersAndCallbacks) {
    const auto result = m_manager->eval(R"-(
        calls = 0
        local function callback() calls = calls + 1 end
        local dispatcher = hl.dsp.no_op()
        assert(type(dispatcher) == 'userdata')
        assert(tostring(dispatcher) == 'HL.Dispatcher(no_op)')
        assert(hl.bind('SUPER + Q', dispatcher) ~= nil)
        assert(hl.bind('SUPER + W', callback) ~= nil)
        assert(hl.bind('SUPER + E', collectgarbage) ~= nil)
        assert(hl.dispatch(dispatcher).ok)
        assert(hl.dispatch(callback).ok)
        assert(calls == 1)
    )-");
    ASSERT_FALSE(result.has_value()) << result.value_or("");
    ASSERT_EQ(Keybinds::mgr()->registry().size(), 3);
    for (const auto& bind : Keybinds::mgr()->registry().binds()) {
        EXPECT_TRUE(bind->invoke().success);
    }

    const auto callbackResult = m_manager->eval("assert(calls == 2)");
    EXPECT_FALSE(callbackResult.has_value()) << callbackResult.value_or("");
    EXPECT_EQ(lua_gettop(m_lua), 0);
}

TEST_F(CConfigLuaDispatchers, rejectsInvalidDispatcherValues) {
    for (const auto* value : {"nil", "42", "{}", "'invalid'"}) {
        SCOPED_TRACE(value);
        const auto result = m_manager->eval(std::format("hl.bind('SUPER + Q', {})", value));
        ASSERT_TRUE(result.has_value());
        EXPECT_NE(result->find("hl.bind: expected a dispatcher"), std::string::npos);
        EXPECT_TRUE(Keybinds::mgr()->registry().empty());
        EXPECT_EQ(lua_gettop(m_lua), 0);
    }
}

TEST_F(CConfigLuaDispatchers, enforcesFactoryArgumentLimits) {
    struct SFactoryCase {
        std::string_view name;
        std::string_view args;
        int              maxArgs = 1;
    };

    static constexpr SFactoryCase CASES[] = {
        {"cursor.move_to_corner", "{ corner = 0 }"},
        {"cursor.move", "{ x = 10, y = 20 }"},
        {"group.toggle", "{}"},
        {"group.next", "{}"},
        {"group.prev", "{}"},
        {"group.active", "{ index = 1 }"},
        {"group.move_window", "{ forward = true }"},
        {"group.lock", "{}"},
        {"group.lock_active", "{}"},
        {"window.close", "{}"},
        {"window.kill", "{}"},
        {"window.signal", "{ signal = 15 }"},
        {"window.float", "{}"},
        {"window.fullscreen", "{}"},
        {"window.fullscreen_state", "{ internal = 0, client = 0 }"},
        {"window.pseudo", "{}"},
        {"window.move", "{ direction = 'left' }"},
        {"window.swap", "{ direction = 'left' }"},
        {"window.center", "{}"},
        {"window.cycle_next", "{}"},
        {"window.tag", "{ tag = 'test' }"},
        {"window.clear_tags", "{}"},
        {"window.toggle_swallow", "", 0},
        {"window.pin", "{}"},
        {"window.bring_to_top", "", 0},
        {"window.alter_zorder", "{ mode = 'top' }"},
        {"window.set_prop", "{ prop = 'opaque', value = '1' }"},
        {"window.deny_from_group", "{}"},
        {"window.drag", "", 0},
        {"window.resize", "{ x = 10, y = 20 }"},
        {"workspace.rename", "{ workspace = '1', name = 'test' }"},
        {"workspace.change_id", "{ workspace = '1', id = 2 }"},
        {"workspace.move", "{ monitor = 'DP-1' }"},
        {"workspace.swap_monitors", "{ monitor1 = 'DP-1', monitor2 = 'DP-2' }"},
        {"workspace.toggle_special", "'test'"},
        {"exec_cmd", "'true', { float = true }", 2},
        {"exec_raw", "'true'"},
        {"exit", "", 0},
        {"reload_config", "", 0},
        {"submap", "'test'"},
        {"pass", "{ window = 'active' }"},
        {"send_shortcut", "{ mods = 'SUPER', key = 'Q' }"},
        {"send_key_state", "{ mods = 'SUPER', key = 'Q', state = 'down' }"},
        {"layout", "'test'"},
        {"dpms", "{}"},
        {"event", "'test'"},
        {"global", "'test:shortcut'"},
        {"force_renderer_reload", "", 0},
        {"force_idle", "1"},
        {"release_input_capture", "", 0},
        {"focus", "{ direction = 'left' }"},
        {"no_op", "", 0},
    };

    for (const auto& test : CASES) {
        SCOPED_TRACE(test.name);
        const auto valid = m_manager->eval(std::format("assert(type(hl.dsp.{}({})) == 'userdata')", test.name, test.args));
        EXPECT_FALSE(valid.has_value()) << valid.value_or("");

        for (const auto* extra : {"{ float = true }", "nil"}) {
            SCOPED_TRACE(extra);
            const auto result = m_manager->eval(std::format("assert(hl.dsp.{}({}{}{}) == nil)", test.name, test.args, test.maxArgs == 0 ? "" : ", ", extra));
            ASSERT_TRUE(result.has_value());
            const auto shortName = test.name.substr(test.name.find_last_of('.') + 1);
            EXPECT_NE(result->find(std::format("{}: expected at most {} argument{}, got {}", shortName, test.maxArgs, test.maxArgs == 1 ? "" : "s", test.maxArgs + 1)),
                      std::string::npos);
            EXPECT_EQ(m_manager->m_errors.size(), 1);
            EXPECT_EQ(lua_gettop(m_lua), 0);
        }
    }
}

TEST_F(CConfigLuaDispatchers, preservesOptionalFactoryArguments) {
    const auto result = m_manager->eval(R"(
        local factories = {
            hl.dsp.group.toggle, hl.dsp.group.next, hl.dsp.group.prev,
            hl.dsp.group.move_window, hl.dsp.group.lock, hl.dsp.group.lock_active,
            hl.dsp.window.close, hl.dsp.window.kill, hl.dsp.window.float,
            hl.dsp.window.fullscreen, hl.dsp.window.pseudo, hl.dsp.window.center,
            hl.dsp.window.cycle_next, hl.dsp.window.clear_tags, hl.dsp.window.pin,
            hl.dsp.window.deny_from_group, hl.dsp.window.resize,
            hl.dsp.workspace.toggle_special, hl.dsp.dpms,
        }
        for _, factory in ipairs(factories) do
            assert(type(factory()) == 'userdata')
            assert(type(factory(nil)) == 'userdata')
        end
        assert(type(hl.dsp.exec_cmd('true')) == 'userdata')
        assert(type(hl.dsp.exec_cmd('true', nil)) == 'userdata')
        assert(type(hl.dsp.exec_raw(123)) == 'userdata')
    )");
    EXPECT_FALSE(result.has_value()) << result.value_or("");
    EXPECT_EQ(lua_gettop(m_lua), 0);
}

TEST_F(CConfigLuaDispatchers, reportsFactoryArgumentErrorsAtCallsite) {
    const auto result = m_manager->eval(R"-(
        local chunk = assert(load("hl.bind('SUPER + Q', hl.dsp.exec_raw('true', { float = true }))", '@dispatcher-arity.lua'))
        chunk()
    )-");
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->find("dispatcher-arity.lua:1: exec_raw: expected at most 1 argument, got 2"), std::string::npos);
    EXPECT_TRUE(Keybinds::mgr()->registry().empty());
    EXPECT_EQ(lua_gettop(m_lua), 0);
}

TEST_F(CConfigLuaDispatchers, preservesRequiredFactoryArgumentChecks) {
    for (const auto* args : {"", "{}"}) {
        SCOPED_TRACE(args);
        const auto result = m_manager->eval(std::format("assert(hl.dsp.exec_raw({}) == nil)", args));
        ASSERT_TRUE(result.has_value());
        EXPECT_NE(result->find("exec_raw: bad argument 1:"), std::string::npos);
        EXPECT_EQ(m_manager->m_errors.size(), 1);
        EXPECT_EQ(lua_gettop(m_lua), 0);
    }
}

TEST(ConfigLuaBindingsInternal, parseDirectionAliases) {
    EXPECT_EQ(Internal::parseDirectionStr("left"), Math::DIRECTION_LEFT);
    EXPECT_EQ(Internal::parseDirectionStr("l"), Math::DIRECTION_LEFT);
    EXPECT_EQ(Internal::parseDirectionStr("right"), Math::DIRECTION_RIGHT);
    EXPECT_EQ(Internal::parseDirectionStr("r"), Math::DIRECTION_RIGHT);
    EXPECT_EQ(Internal::parseDirectionStr("up"), Math::DIRECTION_UP);
    EXPECT_EQ(Internal::parseDirectionStr("t"), Math::DIRECTION_UP);
    EXPECT_EQ(Internal::parseDirectionStr("down"), Math::DIRECTION_DOWN);
    EXPECT_EQ(Internal::parseDirectionStr("b"), Math::DIRECTION_DOWN);
    EXPECT_EQ(Internal::parseDirectionStr("???"), Math::DIRECTION_DEFAULT);
}

TEST(ConfigLuaBindingsInternal, parseToggleAliases) {
    EXPECT_EQ(Internal::parseToggleStr(""), Config::Actions::TOGGLE_ACTION_TOGGLE);
    EXPECT_EQ(Internal::parseToggleStr("toggle"), Config::Actions::TOGGLE_ACTION_TOGGLE);
    EXPECT_EQ(Internal::parseToggleStr("enable"), Config::Actions::TOGGLE_ACTION_ENABLE);
    EXPECT_EQ(Internal::parseToggleStr("on"), Config::Actions::TOGGLE_ACTION_ENABLE);
    EXPECT_EQ(Internal::parseToggleStr("disable"), Config::Actions::TOGGLE_ACTION_DISABLE);
    EXPECT_EQ(Internal::parseToggleStr("off"), Config::Actions::TOGGLE_ACTION_DISABLE);
}

TEST(ConfigLuaBindingsInternal, argStrConvertsStringsAndNumbers) {
    CLuaState  S;
    const auto L = S.get();

    lua_pushstring(L, "abc");
    EXPECT_EQ(Internal::argStr(L, -1), "abc");
    lua_pop(L, 1);

    lua_pushnumber(L, 42);
    EXPECT_EQ(Internal::argStr(L, -1), "42");
    lua_pop(L, 1);
}

TEST(ConfigLuaBindingsInternal, tableOptHelpersReadOptionalFields) {
    CLuaState  S;
    const auto L = S.get();

    lua_createtable(L, 0, 5);
    lua_pushstring(L, "value");
    lua_setfield(L, -2, "s");
    lua_pushnumber(L, 5.5);
    lua_setfield(L, -2, "n");
    lua_pushboolean(L, true);
    lua_setfield(L, -2, "b");
    lua_pushstring(L, "not-number");
    lua_setfield(L, -2, "n2");
    lua_pushnil(L);
    lua_setfield(L, -2, "nilv");

    EXPECT_EQ(Internal::tableOptStr(L, -1, "s").value_or(""), "value");
    EXPECT_DOUBLE_EQ(Internal::tableOptNum(L, -1, "n").value_or(0), 5.5);
    EXPECT_EQ(Internal::tableOptBool(L, -1, "b").value_or(false), true);
    EXPECT_FALSE(Internal::tableOptNum(L, -1, "n2").has_value());
    EXPECT_FALSE(Internal::tableOptStr(L, -1, "missing").has_value());
    EXPECT_FALSE(Internal::tableOptBool(L, -1, "nilv").has_value());

    lua_pop(L, 1);
}

TEST(ConfigLuaBindingsInternal, selectorHelpersAcceptStringAndNumberSelectors) {
    CLuaState  S;
    const auto L = S.get();

    lua_createtable(L, 0, 4);
    lua_pushstring(L, "DP-1");
    lua_setfield(L, -2, "monitor");
    lua_pushnumber(L, 7);
    lua_setfield(L, -2, "workspace");
    lua_pushnumber(L, 1337);
    lua_setfield(L, -2, "window");

    EXPECT_EQ(Internal::tableOptMonitorSelector(L, -1, "monitor", "test.fn").value_or(""), "DP-1");
    EXPECT_EQ(Internal::tableOptWorkspaceSelector(L, -1, "workspace", "test.fn").value_or(""), "7");
    EXPECT_EQ(Internal::tableOptWindowSelector(L, -1, "window", "test.fn").value_or(""), "1337");

    EXPECT_FALSE(Internal::tableOptMonitorSelector(L, -1, "missing", "test.fn").has_value());
    EXPECT_FALSE(Internal::tableOptWorkspaceSelector(L, -1, "missing", "test.fn").has_value());
    EXPECT_FALSE(Internal::tableOptWindowSelector(L, -1, "missing", "test.fn").has_value());

    EXPECT_EQ(Internal::requireTableFieldMonitorSelector(L, -1, "monitor", "test.fn"), "DP-1");
    EXPECT_EQ(Internal::requireTableFieldWorkspaceSelector(L, -1, "workspace", "test.fn"), "7");
    EXPECT_EQ(Internal::requireTableFieldWindowSelector(L, -1, "window", "test.fn"), "1337");

    lua_pop(L, 1);
}

TEST(ConfigLuaBindingsInternal, pushWindowUpvalAcceptsNumberAndStringSelectors) {
    CLuaState  S;
    const auto L = S.get();

    lua_createtable(L, 0, 1);
    lua_pushnumber(L, 42);
    lua_setfield(L, -2, "window");

    Internal::pushWindowUpval(L, -1);
    ASSERT_TRUE(lua_isstring(L, -1));
    EXPECT_STREQ(lua_tostring(L, -1), "42");
    lua_pop(L, 1);

    lua_pushstring(L, "0xabc");
    lua_setfield(L, -2, "window");

    Internal::pushWindowUpval(L, -1);
    ASSERT_TRUE(lua_isstring(L, -1));
    EXPECT_STREQ(lua_tostring(L, -1), "0xabc");
    lua_pop(L, 1);

    lua_pushnil(L);
    lua_setfield(L, -2, "window");

    Internal::pushWindowUpval(L, -1);
    EXPECT_TRUE(lua_isnil(L, -1));
    lua_pop(L, 1);

    lua_pop(L, 1);
}

TEST(ConfigLuaBindingsInternal, parseTableFieldMissingFieldAndPrefixedErrors) {
    CLuaState     S;
    const auto    L = S.get();

    CLuaConfigInt parser(0);

    lua_newtable(L);
    auto err = Internal::parseTableField(L, -1, "required", parser);
    EXPECT_EQ(err.errorCode, PARSE_ERROR_BAD_VALUE);
    EXPECT_NE(err.message.find("missing required field"), std::string::npos);
    lua_pop(L, 1);

    lua_createtable(L, 0, 1);
    lua_pushstring(L, "bad");
    lua_setfield(L, -2, "count");

    err = Internal::parseTableField(L, -1, "count", parser);
    EXPECT_EQ(err.errorCode, PARSE_ERROR_BAD_TYPE);
    EXPECT_NE(err.message.find("field \"count\":"), std::string::npos);
    lua_pop(L, 1);
}

TEST(ConfigLuaBindingsInternal, pluginBindingIsTableWithLoadFunction) {
    CLuaState  S;
    const auto L = S.get();

    lua_newtable(L);
    Internal::registerConfigRuleBindings(L, nullptr);

    lua_getfield(L, -1, "plugin");
    ASSERT_TRUE(lua_istable(L, -1));

    lua_getfield(L, -1, "load");
    EXPECT_TRUE(lua_isfunction(L, -1));
    lua_pop(L, 1);

    lua_pop(L, 2);
}

TEST(ConfigLuaBindingsInternal, getMonitorsReadsOptionsFromArguments) {
    CLuaState  state;
    const auto lua = state.get();

    lua_newtable(lua);
    Internal::registerQueryBindings(lua);
    lua_setglobal(lua, "hl");

    ASSERT_EQ(luaL_dostring(lua, R"(
        assert(type(hl.get_monitors()) == "table")
        assert(type(hl.get_monitors(nil)) == "table")
        assert(type(hl.get_monitors({})) == "table")
        assert(type(hl.get_monitors({ all = false })) == "table")
        assert(type(hl.get_monitors({ all = true })) == "table")
        assert(hl.get_monitors({ all = "true" }) == nil)
        assert(hl.get_monitors(true) == nil)
    )"),
              LUA_OK)
        << lua_tostring(lua, -1);
}

TEST(ConfigLuaBindingsInternal, deprecationNoticesOnlyIncludeUsedDeprecatedValues) {
    CScopedCompositor compositor;
    CLuaState         state;
    const auto        lua = state.get();

    CConfigManager    mgr;
    CConfigManagerPluginLuaTestAccessor::initializeLuaState(mgr, lua);

    lua_newtable(lua);
    Internal::registerConfigRuleBindings(lua, &mgr);
    lua_setglobal(lua, "hl");

    const auto HANDLE = reinterpret_cast<void*>(0x1BADB002);
    ASSERT_TRUE(mgr.registerPluginValue(HANDLE, makeShared<Config::Values::CIntValue>("test:ordinary", "", 0)).has_value());
    ASSERT_TRUE(
        mgr.registerPluginValue(HANDLE, makeShared<Config::Values::CIntValue>("test:deprecated", "", 0, Config::Values::SIntValueOptions{.deprecationNotice = "use replacement"}))
            .has_value());

    EXPECT_TRUE(mgr.deprecationNotices().empty());

    ASSERT_EQ(luaL_dostring(lua, "hl.config({ test = { ordinary = 1 } })"), LUA_OK) << lua_tostring(lua, -1);
    EXPECT_TRUE(mgr.deprecationNotices().empty());

    ASSERT_EQ(luaL_dostring(lua, "hl.config({ test = { deprecated = 1 } })"), LUA_OK) << lua_tostring(lua, -1);

    const auto notices = mgr.deprecationNotices();
    ASSERT_EQ(notices.size(), 1);
    EXPECT_EQ(notices.front(), "test.deprecated: use replacement");
}

TEST(ConfigLuaBindingsInternal, pluginLuaFnIsUnloadedWithoutDanglingCall) {
    CLuaState  S;
    const auto L = S.get();

    auto       PREVCOMPOSITOR = std::move(g_pCompositor);
    g_pCompositor             = makeUnique<CCompositor>(true);

    CConfigManager mgr;
    CConfigManagerPluginLuaTestAccessor::initializeLuaState(mgr, L);

    lua_newtable(L);
    Internal::registerConfigRuleBindings(L, &mgr);
    lua_setglobal(L, "hl");

    const auto HANDLE = reinterpret_cast<void*>(0x1BADB002);

    const auto regResult = mgr.registerPluginLuaFunction(HANDLE, "demo", "ping", testPluginFn);
    ASSERT_TRUE(regResult.has_value()) << regResult.error();

    ASSERT_EQ(luaL_dostring(L, R"(
        local f = hl.plugin.demo.ping
        assert(type(f) == "function")
        captured = f
        local v = f()
        assert(v == "pong")
    )"),
              LUA_OK);

    mgr.onPluginUnload(HANDLE);

    ASSERT_EQ(luaL_dostring(L, R"(
        assert(hl.plugin.demo == nil)
    )"),
              LUA_OK);

    ASSERT_EQ(luaL_dostring(L, R"(
        local ok, err = pcall(captured)
        assert(ok == false)
        assert(type(err) == "string")
        assert(string.find(err, "no longer available", 1, true) ~= nil)
    )"),
              LUA_OK);

    g_pCompositor = std::move(PREVCOMPOSITOR);
}

TEST(ConfigLuaRequire, absolutePathLoadsAndTracksFile) {
    CScopedCompositor compositor;
    CTempDir          tmp;
    const auto        mainConfig = tmp.path() / "hyprland.lua";
    const auto        module     = tmp.path() / "absolute.lua";
    writeFile(mainConfig, "");
    writeFile(module, "return { value = 42 }");

    CConfigManager mgr;
    CConfigManagerPluginLuaTestAccessor::initializeOwnedLuaState(mgr, mainConfig);
    const auto L = CConfigManagerPluginLuaTestAccessor::luaState(mgr);

    const auto CODE = std::format("mod = require({})", luaString(module.string()));
    ASSERT_EQ(luaL_dostring(L, CODE.c_str()), LUA_OK) << lua_tostring(L, -1);

    lua_getglobal(L, "mod");
    ASSERT_TRUE(lua_istable(L, -1));
    lua_getfield(L, -1, "value");
    EXPECT_EQ(lua_tointeger(L, -1), 42);
    lua_pop(L, 2);

    expectTracked(mgr, module);
}

TEST(ConfigLuaRequire, relativePathResolvesFromConfigDirectory) {
    CScopedCompositor compositor;
    CTempDir          tmp;
    const auto        mainConfig = tmp.path() / "hyprland.lua";
    const auto        module     = tmp.path() / "modules" / "relative.lua";
    writeFile(mainConfig, "");
    writeFile(module, "return 'relative-ok'");

    CConfigManager mgr;
    CConfigManagerPluginLuaTestAccessor::initializeOwnedLuaState(mgr, mainConfig);
    const auto L = CConfigManagerPluginLuaTestAccessor::luaState(mgr);

    ASSERT_EQ(luaL_dostring(L, R"(
        mod = require("./modules/relative.lua")
    )"),
              LUA_OK)
        << lua_tostring(L, -1);

    lua_getglobal(L, "mod");
    ASSERT_TRUE(lua_isstring(L, -1));
    EXPECT_STREQ(lua_tostring(L, -1), "relative-ok");
    lua_pop(L, 1);

    expectTracked(mgr, module);
}

TEST(ConfigLuaRequire, wildcardLoadsSortedTableAndTracksFilesAndDirectory) {
    CScopedCompositor compositor;
    CTempDir          tmp;
    const auto        mainConfig = tmp.path() / "hyprland.lua";
    const auto        modulesDir = tmp.path() / "modules";
    const auto        moduleA    = modulesDir / "a.lua";
    const auto        moduleB    = modulesDir / "b.lua";
    writeFile(mainConfig, "");
    writeFile(moduleB, "return 'b'");
    writeFile(moduleA, "return 'a'");

    CConfigManager mgr;
    CConfigManagerPluginLuaTestAccessor::initializeOwnedLuaState(mgr, mainConfig);
    const auto L = CConfigManagerPluginLuaTestAccessor::luaState(mgr);

    ASSERT_EQ(luaL_dostring(L, R"(
        mods = require("./modules/*")
        assert(#mods == 2)
        assert(mods[1] == "a")
        assert(mods[2] == "b")
    )"),
              LUA_OK)
        << lua_tostring(L, -1);

    expectTracked(mgr, modulesDir);
    expectTracked(mgr, moduleA);
    expectTracked(mgr, moduleB);
}

TEST(ConfigLuaRequire, wildcardNoMatchIsCatchableError) {
    CScopedCompositor compositor;
    CTempDir          tmp;
    const auto        mainConfig = tmp.path() / "hyprland.lua";
    writeFile(mainConfig, "");

    CConfigManager mgr;
    CConfigManagerPluginLuaTestAccessor::initializeOwnedLuaState(mgr, mainConfig);
    const auto L = CConfigManagerPluginLuaTestAccessor::luaState(mgr);

    ASSERT_EQ(luaL_dostring(L, R"(
        ok, err = pcall(require, "./missing/*")
        assert(ok == false)
        assert(type(err) == "string")
        assert(string.find(err, "module './missing/*' not found", 1, true) ~= nil)
    )"),
              LUA_OK)
        << lua_tostring(L, -1);
}

TEST(ConfigLuaRequire, normalModuleRequireStillUsesConfigDirectoryPackagePath) {
    CScopedCompositor compositor;
    CTempDir          tmp;
    const auto        mainConfig = tmp.path() / "hyprland.lua";
    const auto        module     = tmp.path() / "colors.lua";
    writeFile(mainConfig, "");
    writeFile(module, "return 'normal-ok'");

    CConfigManager mgr;
    CConfigManagerPluginLuaTestAccessor::initializeOwnedLuaState(mgr, mainConfig);
    const auto L = CConfigManagerPluginLuaTestAccessor::luaState(mgr);

    ASSERT_EQ(luaL_dostring(L, R"(
        mod = require("colors")
    )"),
              LUA_OK)
        << lua_tostring(L, -1);

    lua_getglobal(L, "mod");
    ASSERT_TRUE(lua_isstring(L, -1));
    EXPECT_STREQ(lua_tostring(L, -1), "normal-ok");
    lua_pop(L, 1);

    expectTracked(mgr, module);
}

TEST(ConfigLuaRequire, packagePathPreservesLuaDefaultsAfterConfigDirectory) {
    CScopedCompositor compositor;
    CLuaState         defaultState;
    CTempDir          tmp;
    const auto        mainConfig = tmp.path() / "hyprland.lua";
    writeFile(mainConfig, "");

    const auto defaultPath = packagePath(defaultState.get());
    ASSERT_FALSE(defaultPath.empty());

    CConfigManager mgr;
    CConfigManagerPluginLuaTestAccessor::initializeOwnedLuaState(mgr, mainConfig);

    const auto configPath = std::format("{};{}", (tmp.path() / "?.lua").string(), (tmp.path() / "?/init.lua").string());
    EXPECT_EQ(packagePath(CConfigManagerPluginLuaTestAccessor::luaState(mgr)), std::format("{};{}", configPath, defaultPath));
}
