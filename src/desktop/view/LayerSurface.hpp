#pragma once

#include <string>
#include "../../defines.hpp"
#include "WLSurface.hpp"
#include "View.hpp"
#include "../rule/layerRule/LayerRuleApplicator.hpp"
#include "../../helpers/AnimatedVariable.hpp"

class CLayerShellResource;

namespace Desktop::View {

    class CLayerSurface : public IView {
      public:
        static PHLLS create(SP<CLayerShellResource>);
        static PHLLS fromView(SP<IView>);

      private:
        CLayerSurface(SP<CLayerShellResource>);

      public:
        virtual ~CLayerSurface();

        virtual eViewType           type() const;
        virtual bool                visible() const;
        virtual std::optional<CBox> logicalBox() const;
        virtual bool                desktopComponent() const;
        virtual std::optional<CBox> surfaceLogicalBox() const;

        bool                        isFadedOut();
        int                         popupsCount();

        PHLANIMVAR<Vector2D>        m_realPosition;
        PHLANIMVAR<Vector2D>        m_realSize;
        PHLANIMVAR<float>           m_alpha;

        WP<CLayerShellResource>     m_layerSurface;

        // the header providing the enum type cannot be imported here
        int                                     m_interactivity = 0;

        bool                                    m_mapped = false;
        uint32_t                                m_layer  = 0;

        PHLMONITORREF                           m_monitor;

        bool                                    m_fadingOut       = false;
        bool                                    m_readyToDelete   = false;
        bool                                    m_noProcess       = false;
        bool                                    m_aboveFullscreen = true;

        UP<Desktop::Rule::CLayerRuleApplicator> m_ruleApplicator;

        PHLLSREF                                m_self;

        CBox                                    m_geometry = {0, 0, 0, 0};
        Vector2D                                m_position;
        std::string                             m_namespace = "";
        SP<Desktop::View::CPopup>               m_popupHead;

        pid_t                                   getPID();

        void                                    onDestroy();
        void                                    onMap();
        void                                    onUnmap();
        void                                    onCommit();
        MONITORID                               monitorID();

      private:
        struct {
            CHyprSignalListener destroy;
            CHyprSignalListener map;
            CHyprSignalListener unmap;
            CHyprSignalListener commit;
        } m_listeners;

        void registerCallbacks();

        // For the list lookup
        bool operator==(const CLayerSurface& rhs) const {
            return m_layerSurface == rhs.m_layerSurface && m_monitor == rhs.m_monitor;
        }
    };

    inline bool valid(PHLLS l) {
        return l;
    }

    inline bool valid(PHLLSREF l) {
        return l;
    }

    inline bool validMapped(PHLLS l) {
        if (!valid(l))
            return false;
        return l->aliveAndVisible();
    }

    inline bool validMapped(PHLLSREF l) {
        if (!valid(l))
            return false;
        return l->aliveAndVisible();
    }

}
