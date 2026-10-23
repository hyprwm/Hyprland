#include <Compositor.hpp>
#include <config/RedirectableConfigValue.hpp>
#include <config/lua/ConfigManager.hpp>
#include <config/lua/types/LuaConfigBool.hpp>
#include <config/lua/types/LuaConfigInt.hpp>
#include <config/lua/types/LuaConfigString.hpp>
#include <config/shared/inotify/ConfigWatcher.hpp>
#include <config/supplementary/jeremy/Jeremy.hpp>

#include <gtest/gtest.h>
#include <format>
#include <memory>

using namespace Config::Lua;

class CConfigValueTestManager : public CConfigManager {
  public:
    Config::SConfigOptionReply getConfigValue(const std::string& name) override {
        ++m_lookups;
        return CConfigManager::getConfigValue(name);
    }

    size_t m_lookups = 0;
};

class CRedirectableConfigValueTest : public testing::Test {
  protected:
    void SetUp() override {
        ASSERT_NE(m_lua, nullptr);

        m_previousCompositor = std::move(g_pCompositor);
        m_previousConfig     = std::move(Config::mgr());
        m_previousWatcher    = std::move(Config::watcher());
        m_installedGlobals   = true;

        // Only construct config defaults; no config file or compositor backend is loaded.
        g_pCompositor                       = makeUnique<CCompositor>(true);
        g_pCompositor->m_explicitConfigPath = "redirectable-config-value-test.lua";
        Config::Supplementary::Jeremy::flushCachedCfgPath();
        Config::watcher() = makeUnique<Config::CConfigWatcher>();
        auto manager      = makeUnique<CConfigValueTestManager>();
        m_manager         = manager;
        Config::mgr()     = std::move(manager);
        CConfigValueBase::flushCaches();

        m_manager->m_configValues.emplace(TARGET, makeUnique<CLuaConfigInt>(10, 0, 100));
        m_manager->m_configValues.emplace(FALLBACK, makeUnique<CLuaConfigInt>(20, 0, 100));
    }

    void TearDown() override {
        if (!m_installedGlobals)
            return;

        Config::mgr().reset();
        Config::watcher().reset();
        Config::mgr()     = std::move(m_previousConfig);
        Config::watcher() = std::move(m_previousWatcher);
        if (Config::mgr())
            CConfigValueBase::flushCaches();
        g_pCompositor.reset();
        g_pCompositor = std::move(m_previousCompositor);
        Config::Supplementary::Jeremy::flushCachedCfgPath();
    }

    SParseError parseTop(const std::string& name) {
        const auto result = m_manager->m_configValues.at(name)->parse(m_lua.get());
        lua_pop(m_lua.get(), 1);
        return result;
    }

    SParseError parseInt(const std::string& name, Config::INTEGER value) {
        lua_pushinteger(m_lua.get(), value);
        return parseTop(name);
    }

    void reset(const std::string& name) {
        auto& value = m_manager->m_configValues.at(name);
        value->reset();
        value->resetSetByUser();
    }

    static constexpr auto                            TARGET   = "test.redirect.target_value";
    static constexpr auto                            FALLBACK = "test.redirect.fallback_value";

    WP<CConfigValueTestManager>                      m_manager;
    std::unique_ptr<lua_State, decltype(&lua_close)> m_lua{luaL_newstate(), lua_close};

  private:
    bool                       m_installedGlobals = false;
    UP<CCompositor>            m_previousCompositor;
    UP<Config::IConfigManager> m_previousConfig;
    UP<Config::CConfigWatcher> m_previousWatcher;
};

TEST_F(CRedirectableConfigValueTest, selectsByExplicitnessWithoutLookingUpOnRead) {
    const CRedirectableConfigValue<Config::INTEGER> value(TARGET, FALLBACK);
    const CConfigValue<Config::INTEGER>             target(TARGET);
    const auto                                      lookups = m_manager->m_lookups;
    ASSERT_TRUE(value.good());

    for (const bool targetSet : {false, true}) {
        for (const bool fallbackSet : {false, true}) {
            SCOPED_TRACE(std::format("target set: {}, fallback set: {}", targetSet, fallbackSet));
            reset(TARGET);
            reset(FALLBACK);
            if (fallbackSet) {
                const auto result = parseInt(FALLBACK, 47);
                ASSERT_EQ(result.errorCode, PARSE_ERROR_OK) << result.message;
            }
            if (targetSet) {
                const auto result = parseInt(TARGET, 31);
                ASSERT_EQ(result.errorCode, PARSE_ERROR_OK) << result.message;
            }

            EXPECT_EQ(*value, targetSet ? 31 : (fallbackSet ? 47 : 20));
            EXPECT_EQ(target.setByUser(), targetSet);
        }
    }

    EXPECT_EQ(m_manager->m_lookups, lookups);
}

TEST_F(CRedirectableConfigValueTest, explicitDefaultAndZeroOverrideFallback) {
    const CRedirectableConfigValue<Config::INTEGER> value(TARGET, FALLBACK);
    ASSERT_EQ(parseInt(FALLBACK, 47).errorCode, PARSE_ERROR_OK);

    for (const auto explicitValue : {10, 0}) {
        SCOPED_TRACE(explicitValue);
        ASSERT_EQ(parseInt(TARGET, explicitValue).errorCode, PARSE_ERROR_OK);
        EXPECT_EQ(*value, explicitValue);
    }
}

TEST_F(CRedirectableConfigValueTest, explicitFalseOverridesFallbackIncludingIntegerCompatibility) {
    m_manager->m_configValues.at(TARGET)   = makeUnique<CLuaConfigBool>(false);
    m_manager->m_configValues.at(FALLBACK) = makeUnique<CLuaConfigBool>(true);

    const CRedirectableConfigValue<Config::BOOL>    boolean(TARGET, FALLBACK);
    const CRedirectableConfigValue<Config::INTEGER> integer(TARGET, FALLBACK);
    EXPECT_TRUE(*boolean);
    EXPECT_EQ(*integer, 1);

    lua_pushboolean(m_lua.get(), false);
    ASSERT_EQ(parseTop(TARGET).errorCode, PARSE_ERROR_OK);
    EXPECT_FALSE(*boolean);
    EXPECT_EQ(*integer, 0);
}

TEST_F(CRedirectableConfigValueTest, explicitEmptyStringOverridesFallback) {
    m_manager->m_configValues.at(TARGET)   = makeUnique<CLuaConfigString>("");
    m_manager->m_configValues.at(FALLBACK) = makeUnique<CLuaConfigString>("legacy");

    const CRedirectableConfigValue<Config::STRING> value(TARGET, FALLBACK);
    EXPECT_EQ(*value, "legacy");

    lua_pushstring(m_lua.get(), "");
    ASSERT_EQ(parseTop(TARGET).errorCode, PARSE_ERROR_OK);
    EXPECT_EQ(*value, "");
}

TEST_F(CRedirectableConfigValueTest, transformsOnlyFallbackAndReadsCurrentValues) {
    size_t                                          calls = 0;
    const CRedirectableConfigValue<Config::INTEGER> value(TARGET, FALLBACK, [&calls](Config::INTEGER fallback) {
        ++calls;
        return 2 * fallback + 1;
    });

    EXPECT_EQ(*value, 41);
    EXPECT_EQ(calls, 1U);
    ASSERT_EQ(parseInt(FALLBACK, 7).errorCode, PARSE_ERROR_OK);
    EXPECT_EQ(*value, 15);
    EXPECT_EQ(calls, 2U);

    ASSERT_EQ(parseInt(TARGET, 0).errorCode, PARSE_ERROR_OK);
    EXPECT_EQ(*value, 0);
    ASSERT_EQ(parseInt(FALLBACK, 8).errorCode, PARSE_ERROR_OK);
    EXPECT_EQ(*value, 0);
    EXPECT_EQ(calls, 2U);

    m_manager->m_configValues.at(TARGET)->resetSetByUser();
    EXPECT_EQ(*value, 17);
    EXPECT_EQ(calls, 3U);
    EXPECT_EQ(*value, 17);
    EXPECT_EQ(calls, 4U);
}

TEST_F(CRedirectableConfigValueTest, observesValueAndExplicitnessResetsSeparately) {
    const CRedirectableConfigValue<Config::INTEGER> value(TARGET, FALLBACK);
    const CConfigValue<Config::INTEGER>             target(TARGET);
    ASSERT_EQ(parseInt(FALLBACK, 47).errorCode, PARSE_ERROR_OK);
    ASSERT_EQ(parseInt(TARGET, 31).errorCode, PARSE_ERROR_OK);
    EXPECT_EQ(*value, 31);

    m_manager->m_configValues.at(TARGET)->reset();
    EXPECT_TRUE(target.setByUser());
    EXPECT_EQ(*value, 10);

    m_manager->m_configValues.at(TARGET)->resetSetByUser();
    EXPECT_FALSE(target.setByUser());
    EXPECT_EQ(*value, 47);

    reset(FALLBACK);
    EXPECT_EQ(*value, 20);
}

TEST_F(CRedirectableConfigValueTest, rejectedParsePreservesSelection) {
    const CRedirectableConfigValue<Config::INTEGER> value(TARGET, FALLBACK);
    const CConfigValue<Config::INTEGER>             target(TARGET);
    ASSERT_EQ(parseInt(FALLBACK, 47).errorCode, PARSE_ERROR_OK);

    ASSERT_EQ(parseInt(TARGET, 101).errorCode, PARSE_ERROR_OUT_OF_RANGE);
    EXPECT_FALSE(target.setByUser());
    EXPECT_EQ(*value, 47);

    ASSERT_EQ(parseInt(TARGET, 31).errorCode, PARSE_ERROR_OK);
    ASSERT_EQ(parseInt(TARGET, 101).errorCode, PARSE_ERROR_OUT_OF_RANGE);
    EXPECT_TRUE(target.setByUser());
    EXPECT_EQ(*value, 31);
}

TEST_F(CRedirectableConfigValueTest, cacheFlushRefreshesDataAndExplicitnessForBothBindings) {
    const CRedirectableConfigValue<Config::INTEGER> value(TARGET, FALLBACK);
    const CConfigValue<Config::INTEGER>             target(TARGET);
    ASSERT_EQ(parseInt(TARGET, 31).errorCode, PARSE_ERROR_OK);
    ASSERT_EQ(parseInt(FALLBACK, 47).errorCode, PARSE_ERROR_OK);
    EXPECT_EQ(*value, 31);

    // Keep the old allocations alive so a stale flag fails deterministically, not through a dangling pointer.
    auto oldTarget                         = std::move(m_manager->m_configValues.at(TARGET));
    auto oldFallback                       = std::move(m_manager->m_configValues.at(FALLBACK));
    m_manager->m_configValues.at(TARGET)   = makeUnique<CLuaConfigInt>(8);
    m_manager->m_configValues.at(FALLBACK) = makeUnique<CLuaConfigInt>(9);
    CConfigValueBase::flushCaches();

    EXPECT_FALSE(target.setByUser());
    EXPECT_EQ(*value, 9);
    ASSERT_EQ(parseInt(TARGET, 17).errorCode, PARSE_ERROR_OK);
    EXPECT_TRUE(target.setByUser());
    EXPECT_EQ(*value, 17);

    reset(TARGET);
    ASSERT_EQ(parseInt(FALLBACK, 25).errorCode, PARSE_ERROR_OK);
    EXPECT_EQ(*value, 25);
}

TEST_F(CRedirectableConfigValueTest, rebindRefreshesExplicitnessAlongWithData) {
    CConfigValue<Config::INTEGER> value(TARGET);
    ASSERT_EQ(parseInt(TARGET, 31).errorCode, PARSE_ERROR_OK);
    EXPECT_TRUE(value.setByUser());

    value.bind(FALLBACK);
    EXPECT_FALSE(value.setByUser());
    EXPECT_EQ(*value, 20);
    ASSERT_EQ(parseInt(FALLBACK, 47).errorCode, PARSE_ERROR_OK);
    EXPECT_TRUE(value.setByUser());
    EXPECT_EQ(*value, 47);
}

TEST_F(CRedirectableConfigValueTest, canonicalAndLegacyNamesShareLiveExplicitness) {
    const auto                          canonical = m_manager->getConfigValue(TARGET);
    const auto                          legacy    = m_manager->getConfigValue("test:redirect:target-value");
    const CConfigValue<Config::INTEGER> value("test:redirect:target-value");
    ASSERT_NE(canonical.setByUser, nullptr);
    EXPECT_EQ(canonical.setByUser, legacy.setByUser);
    EXPECT_EQ(canonical.dataptr, legacy.dataptr);
    EXPECT_FALSE(*canonical.setByUser);
    EXPECT_FALSE(value.setByUser());

    ASSERT_EQ(parseInt(TARGET, 0).errorCode, PARSE_ERROR_OK);
    EXPECT_TRUE(*canonical.setByUser);
    EXPECT_TRUE(*legacy.setByUser);
    EXPECT_TRUE(value.setByUser());
    EXPECT_EQ(*value, 0);

    reset(TARGET);
    EXPECT_FALSE(*canonical.setByUser);
    EXPECT_FALSE(*legacy.setByUser);
    EXPECT_FALSE(value.setByUser());
    EXPECT_EQ(*value, 10);
}
