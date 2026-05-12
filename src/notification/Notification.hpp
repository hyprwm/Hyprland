#pragma once

#include "SharedDefines.hpp"
#include "../desktop/DesktopTypes.hpp"
#include "../render/Texture.hpp"
#include "../helpers/time/Time.hpp"

namespace Notification {
    class CNotification {
      public:
        CNotification(std::string&& text, float timeout, CHyprColor color, eIcons icon, float fontSize);

        struct SRenderCache {
            SP<Render::ITexture> textTex;
            SP<Render::ITexture> iconTex;

            Vector2D             textSize = {};
            Vector2D             iconSize = {};

            PHLMONITORREF        monitor;
            std::string          fontFamily;
            int                  fontSizePx  = -1;
            eIconBackend         iconBackend = ICONS_BACKEND_NONE;
        } m_cache;

        void               setText(std::string&& text);
        void               setColor(CHyprColor color);
        void               setIcon(eIcons icon);
        void               setFontSize(float fontSize);
        void               resetTimeout(float timeMs);

        const std::string& text() const;
        const CHyprColor&  color() const;
        eIcons             icon() const;
        float              fontSize() const;
        float              timeMs() const;

        float              timeElapsedSinceCreationMs() const;
        float              timeElapsedMs() const;
        bool               gone() const;

        // lock the notification - stops the timer.
        bool isLocked() const;
        void lock();
        void unlock();

      private:
        const Time::steady_tp m_createdAt = Time::steadyNow();

        std::string           m_text = "";
        CHyprColor            m_color;

        eIcons                m_icon     = ICON_NONE;
        float                 m_fontSize = 13.F;

        Time::steady_tp       m_startedAt        = Time::steadyNow();
        float                 m_timeMs           = 0.F;
        float                 m_lastPauseElapsed = 0.F;

        size_t                m_pauseLocks = 0;
    };
}