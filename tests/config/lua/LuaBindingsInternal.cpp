#include <config/lua/bindings/LuaBindingsInternal.hpp>
#include <config/lua/ConfigManager.hpp>

#include <Compositor.hpp>

#include <config/lua/types/LuaConfigInt.hpp>
#include <config/shared/actions/ConfigActions.hpp>
#include <config/values/types/IntValue.hpp>
#include <overview/Overview.hpp>

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
    class CTestOverview final : public Overview::IOverview, public Overview::IOverviewNavigable, public Overview::IOverviewQueryOpenable, public Overview::IOverviewStateProvider {
      public:
        virtual void open(PHLMONITOR) override {
            m_open = true;
        }

        virtual void open(PHLMONITOR, const std::string& query) override {
            m_open  = true;
            m_query = query;
        }

        virtual void close() override {
            m_open = false;
        }

        virtual bool isOpen() const override {
            return m_open;
        }

        virtual Overview::SOverviewState state() const override {
            return {.open = m_open, .query = m_open ? m_query : ""};
        }

        virtual bool moveLeft() override {
            m_moves--;
            return true;
        }

        virtual bool moveRight() override {
            m_moves++;
            return true;
        }

        bool        m_open  = false;
        int         m_moves = 0;
        std::string m_query;
    };

    class CScopedTestOverview {
      public:
        CScopedTestOverview() : m_previous(std::move(Overview::overview())) {
            Overview::overview() = makeUnique<CTestOverview>();
        }

        ~CScopedTestOverview() {
            Overview::overview() = std::move(m_previous);
        }

        CTestOverview& get() const {
            return *dynamic_cast<CTestOverview*>(Overview::overview().get());
        }

      private:
        UP<Overview::IOverview> m_previous;
    };

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

TEST(ConfigLuaBindingsInternal, dispatcherRegistrationIncludesOverviewTable) {
    CLuaState  S;
    const auto L = S.get();

    lua_newtable(L);
    Internal::registerDispatcherBindings(L);

    lua_getfield(L, 1, "dsp");
    ASSERT_TRUE(lua_istable(L, -1));

    lua_getfield(L, -1, "overview");
    ASSERT_TRUE(lua_istable(L, -1));
    lua_getfield(L, -1, "toggle");
    EXPECT_TRUE(lua_isfunction(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, -1, "move_left");
    EXPECT_TRUE(lua_isfunction(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, -1, "move_right");
    EXPECT_TRUE(lua_isfunction(L, -1));
    lua_pop(L, 2);

    lua_getfield(L, -1, "no_op");
    EXPECT_TRUE(lua_isfunction(L, -1));
}

TEST(ConfigLuaBindingsInternal, overviewActionsForwardQueryAndNavigation) {
    CScopedTestOverview overview;

    ASSERT_TRUE(Config::Actions::overview(Config::Actions::TOGGLE_ACTION_ENABLE, "query with spaces"));
    EXPECT_TRUE(overview.get().m_open);
    EXPECT_EQ(overview.get().m_query, "query with spaces");

    EXPECT_TRUE(Config::Actions::overviewMoveLeft());
    EXPECT_TRUE(Config::Actions::overviewMoveRight());
    EXPECT_EQ(overview.get().m_moves, 0);

    ASSERT_TRUE(Config::Actions::overview(Config::Actions::TOGGLE_ACTION_DISABLE));
    EXPECT_FALSE(overview.get().m_open);
    const auto moveResult = Config::Actions::overviewMoveLeft();
    ASSERT_FALSE(moveResult);
    EXPECT_EQ(moveResult.error().code, Config::Actions::eActionErrorCode::INVALID_STATE);
}

TEST(ConfigLuaBindingsInternal, overviewToggleDispatcherPreservesQuery) {
    CScopedTestOverview overview;
    CLuaState           state;
    const auto          lua = state.get();

    lua_newtable(lua);
    Internal::registerDispatcherBindings(lua);
    lua_setglobal(lua, "hl");

    lua_getglobal(lua, "hl");
    lua_getfield(lua, -1, "dsp");
    lua_getfield(lua, -1, "overview");
    lua_getfield(lua, -1, "toggle");
    lua_createtable(lua, 0, 1);
    constexpr char QUERY[] = {'q', '\0', 'x'};
    lua_pushlstring(lua, QUERY, sizeof(QUERY));
    lua_setfield(lua, -2, "query");
    lua_call(lua, 1, 1);

    ASSERT_TRUE(Internal::pushDispatcherFunction(lua, -1));
    lua_call(lua, 0, 1);
    EXPECT_TRUE(overview.get().m_open);
    EXPECT_EQ(overview.get().m_query, std::string(QUERY, sizeof(QUERY)));

    ASSERT_EQ(luaL_dostring(lua, "assert(hl.dsp.overview.toggle({ query = true }) == nil)"), LUA_OK) << lua_tostring(lua, -1);
    ASSERT_EQ(luaL_dostring(lua, "assert(hl.dsp.overview.toggle({ query = 42 }) == nil)"), LUA_OK) << lua_tostring(lua, -1);
}

TEST(ConfigLuaBindingsInternal, getOverviewReturnsStableStateTable) {
    CScopedTestOverview overview;
    CLuaState           state;
    const auto          lua = state.get();

    lua_newtable(lua);
    Internal::registerQueryBindings(lua);
    lua_setglobal(lua, "hl");

    ASSERT_EQ(luaL_dostring(lua, R"(
        local overview = hl.get_overview()
        assert(type(overview) == "table")
        assert(overview.open == false)
        assert(overview.monitor == nil)
        assert(overview.workspace == nil)
        assert(overview.query == "")
    )"),
              LUA_OK)
        << lua_tostring(lua, -1);

    overview.get().m_open = true;
    ASSERT_EQ(luaL_dostring(lua, "assert(hl.get_overview().open == true)"), LUA_OK) << lua_tostring(lua, -1);
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
