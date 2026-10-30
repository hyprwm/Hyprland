#include <Compositor.hpp>
#include <managers/TokenManager.hpp>
#include <config/ConfigValue.hpp>
#include <config/lua/ConfigManager.hpp>
#include <config/shared/inotify/ConfigWatcher.hpp>
#include <config/supplementary/jeremy/Jeremy.hpp>
#include <protocols/LinuxDMABUF.hpp>

#include <gtest/gtest.h>

#include <unistd.h>

// Synthetic format lists exercise feedback policy without a DRM device or GPU.
class CDMABUFFormatTableTest : public ::testing::Test {
  protected:
    void SetUp() override {
        m_previousCompositor                = std::move(g_pCompositor);
        m_previousConfig                    = std::move(Config::mgr());
        m_previousWatcher                   = std::move(Config::watcher());
        m_previousTokens                    = std::move(g_pTokenManager);
        g_pTokenManager                     = makeUnique<CTokenManager>();
        g_pCompositor                       = makeUnique<CCompositor>(true);
        g_pCompositor->m_explicitConfigPath = "dmabuf-feedback-test.lua";
        Config::Supplementary::Jeremy::flushCachedCfgPath();
        Config::watcher() = makeUnique<Config::CConfigWatcher>();
        Config::mgr()     = makeUnique<Config::Lua::CConfigManager>();
        CConfigValueBase::flushCaches();
    }

    void TearDown() override {
        Config::mgr()     = std::move(m_previousConfig);
        Config::watcher() = std::move(m_previousWatcher);
        if (Config::mgr())
            CConfigValueBase::flushCaches();
        g_pCompositor   = std::move(m_previousCompositor);
        g_pTokenManager = std::move(m_previousTokens);
        Config::Supplementary::Jeremy::flushCachedCfgPath();
    }

    void setFiltering(bool enabled) {
        CConfigValue<Config::BOOL> filtering{"quirks:skip_non_kms_dmabuf_formats"};
        *filtering.ptr() = enabled;
    }

    UP<CDMABUFFormatTable> makeTable(bool withMonitor = true) {
        SDMABUFTranche renderer{
            .device = 1,
            .formats =
                {
                    {DRM_FORMAT_ARGB8888, {I915_FORMAT_MOD_X_TILED}},
                    {DRM_FORMAT_XRGB8888, {I915_FORMAT_MOD_X_TILED, DRM_FORMAT_MOD_LINEAR}},
                },
        };
        std::vector<std::pair<PHLMONITORREF, SDMABUFTranche>> monitors;
        if (withMonitor)
            monitors.emplace_back(PHLMONITORREF{},
                                  SDMABUFTranche{
                                      .device  = 1,
                                      .flags   = ZWP_LINUX_DMABUF_FEEDBACK_V1_TRANCHE_FLAGS_SCANOUT,
                                      .formats = {{DRM_FORMAT_XRGB8888, {I915_FORMAT_MOD_X_TILED}}},
                                  });
        return makeUnique<CDMABUFFormatTable>(renderer, monitors);
    }

    void expectFormats(const CDMABUFFormatTable& table, const SDMABUFTranche& tranche, const std::vector<std::pair<uint32_t, uint64_t>>& expected) {
        ASSERT_TRUE(table.m_tableFD.isValid());
        std::vector<SDMABUFFormatTableEntry> entries(table.m_tableSize / sizeof(SDMABUFFormatTableEntry));
        ASSERT_EQ(pread(table.m_tableFD.get(), entries.data(), table.m_tableSize, 0), sc<ssize_t>(table.m_tableSize));
        std::vector<std::pair<uint32_t, uint64_t>> actual;
        for (const auto index : tranche.indices) {
            ASSERT_LT(index, entries.size());
            actual.emplace_back(entries[index].fmt, entries[index].modifier);
        }
        EXPECT_EQ(actual, expected);
    }

  private:
    UP<CCompositor>            m_previousCompositor;
    UP<Config::IConfigManager> m_previousConfig;
    UP<Config::CConfigWatcher> m_previousWatcher;
    UP<CTokenManager>          m_previousTokens;
};

TEST_F(CDMABUFFormatTableTest, DefaultRetainsAlphaForOpaqueOnlyKMS) {
    const auto table = makeTable();
    expectFormats(*table, table->m_rendererTranche,
                  {
                      {DRM_FORMAT_ARGB8888, I915_FORMAT_MOD_X_TILED},
                      {DRM_FORMAT_XRGB8888, I915_FORMAT_MOD_X_TILED},
                      {DRM_FORMAT_XRGB8888, DRM_FORMAT_MOD_LINEAR},
                  });
    expectFormats(*table, table->m_monitorTranches.front().second, {{DRM_FORMAT_XRGB8888, I915_FORMAT_MOD_X_TILED}});
}

TEST_F(CDMABUFFormatTableTest, ExplicitFilteringRequiresExactFormatAndModifier) {
    setFiltering(true);
    const auto table = makeTable();
    expectFormats(*table, table->m_rendererTranche, {{DRM_FORMAT_XRGB8888, I915_FORMAT_MOD_X_TILED}});
    expectFormats(*table, table->m_monitorTranches.front().second, {{DRM_FORMAT_XRGB8888, I915_FORMAT_MOD_X_TILED}});
}

TEST_F(CDMABUFFormatTableTest, DisablingFilteringRestoresRendererFormats) {
    setFiltering(true);
    setFiltering(false);
    const auto table = makeTable();
    expectFormats(*table, table->m_rendererTranche,
                  {
                      {DRM_FORMAT_ARGB8888, I915_FORMAT_MOD_X_TILED},
                      {DRM_FORMAT_XRGB8888, I915_FORMAT_MOD_X_TILED},
                      {DRM_FORMAT_XRGB8888, DRM_FORMAT_MOD_LINEAR},
                  });
}

TEST_F(CDMABUFFormatTableTest, FilteringWithoutMonitorsRetainsRendererFormats) {
    setFiltering(true);
    const auto table = makeTable(false);
    expectFormats(*table, table->m_rendererTranche,
                  {
                      {DRM_FORMAT_ARGB8888, I915_FORMAT_MOD_X_TILED},
                      {DRM_FORMAT_XRGB8888, I915_FORMAT_MOD_X_TILED},
                      {DRM_FORMAT_XRGB8888, DRM_FORMAT_MOD_LINEAR},
                  });
}
