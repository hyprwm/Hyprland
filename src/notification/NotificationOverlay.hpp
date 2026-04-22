#pragma once

#include "SharedDefines.hpp"
#include "Notification.hpp"

#include "../desktop/DesktopTypes.hpp"

namespace Notification {

    class CNotificationOverlay {
      public:
        CNotificationOverlay();
        ~CNotificationOverlay();

        void              draw(PHLMONITOR pMonitor);
        SP<CNotification> addNotification(const std::string& text, const CHyprColor& color, const float timeMs, const eIcons icon = ICON_NONE, const float fontSize = 13.f);
        void              dismissNotifications(const int amount);
        void              dismissNotification(const SP<CNotification>& notification);

        std::vector<SP<CNotification>> getNotifications() const;

        bool                           hasAny();

      private:
        void                           ensureNotificationCache(CNotification& notif, PHLMONITOR pMonitor, const std::string& fontFamily);
        eIconBackend                   iconBackendForFont(const std::string& fontFamily);
        CBox                           notificationDamageForMonitor(PHLMONITOR pMonitor);
        CBox                           drawNotifications(PHLMONITOR pMonitor);
        void                           scheduleFrames() const;
        CBox                           m_lastDamage;

        std::vector<SP<CNotification>> m_notifications;

        std::string                    m_cachedIconBackendFontFamily;
        eIconBackend                   m_cachedIconBackend = ICONS_BACKEND_NONE;
        bool                           m_iconBackendValid  = false;
    };

    UP<CNotificationOverlay>& overlay();
}
