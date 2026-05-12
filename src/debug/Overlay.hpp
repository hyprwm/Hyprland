#pragma once

#include "../defines.hpp"
#include "../render/Texture.hpp"
#include "../helpers/time/Timer.hpp"
#include <map>
#include <deque>
#include <string>
#include <vector>

namespace Render {
    class IHyprRenderer;
    class ITexture;
}

namespace Debug {

    class CMonitorOverlay {
      public:
        int         draw(int offset, bool& cacheUpdated);
        const CBox& lastDrawnBox() const;

        void        renderData(PHLMONITOR pMonitor, float durationUs);
        void        renderDataNoOverlay(PHLMONITOR pMonitor, float durationUs);
        void        frameData(PHLMONITOR pMonitor);

        Vector2D    size() const;

      private:
        struct STextLine {
            std::string          text;
            CHyprColor           color;
            int                  fontSize = 10;
            SP<Render::ITexture> texture;
        };

        struct SMetricData {
            float avg = 0.F;
            float min = 0.F;
            float max = 0.F;
            float var = 0.F;
        };

        void                                           updateLine(size_t idx, const std::string& text, const CHyprColor& color, int fontSize, const std::string& fontFamily);
        void                                           rebuildCache();

        std::deque<float>                              m_lastFrametimes;
        std::deque<float>                              m_lastFPSPerSecond;
        std::deque<float>                              m_lastRenderTimes;
        std::deque<float>                              m_lastRenderTimesNoOverlay;
        std::deque<float>                              m_lastAnimationTicks;
        std::chrono::high_resolution_clock::time_point m_lastFrame;
        std::chrono::high_resolution_clock::time_point m_fpsSecondStart;
        size_t                                         m_framesInCurrentSecond = 0;
        PHLMONITORREF                                  m_monitor;

        std::vector<STextLine>                         m_cachedLines;
        bool                                           m_cacheValid = false;
        CTimer                                         m_cacheTimer;

        CBox                                           m_lastDrawnBox;

        friend class Render::IHyprRenderer;
    };

    class COverlay {
      public:
        COverlay();
        void draw();
        void renderData(PHLMONITOR, float durationUs);
        void renderDataNoOverlay(PHLMONITOR, float durationUs);
        void frameData(PHLMONITOR);

      private:
        std::map<PHLMONITORREF, CMonitorOverlay> m_monitorOverlays;
        CBox                                     m_lastDrawnBox;
        CTimer                                   m_frameTimer;

        void                                     createWarningTexture(float maxW);
        SP<Render::ITexture>                     m_warningTexture;
        float                                    m_warningTextureMaxW = 0;

        friend class CHyprMonitorDebugOverlay;
        friend class Render::IHyprRenderer;
    };

    UP<COverlay>& overlay();
}
