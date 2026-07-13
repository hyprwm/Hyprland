#pragma once

#include <string>
#include "../../defines.hpp"
#include "WLSurface.hpp"
#include "View.hpp"
#include "../rule/layerRule/LayerRuleApplicator.hpp"
#include "../../helpers/AnimatedVariable.hpp"
#include "../../render/Framebuffer.hpp"
#include "types/GeometricMovableAnimated.hpp"
#include "types/AlphaModifiable.hpp"
#include "animationControllers/LayerSurfaceAnimationController.hpp"

class CLayerShellResource;

namespace Desktop::View {

    enum eLayerAlpha : uint8_t {
        LS_ALPHA_FADE = 0,

        LS_ALPHA_LAST,
    };

    class CLayerSurface : public virtual IView, public virtual CGeometricMovableAnimated, public virtual IAlphaModifiable {
      public:
        static PHLLS create(SP<CLayerShellResource>);
        static PHLLS fromView(SP<IView>);

      private:
        CLayerSurface(SP<CLayerShellResource>);

      public:
        virtual ~CLayerSurface();

        virtual eViewType                                   type() const override;
        virtual bool                                        visible() const override;
        virtual std::optional<CBox>                         logicalBox() const override;
        virtual bool                                        desktopComponent() const override;
        virtual std::optional<CBox>                         surfaceLogicalBox() const override;
        virtual Types::CMultiAVarContainer<float, uint8_t>& alpha() override;
        virtual std::optional<uint8_t>                      alphaGenericToKey(eAlphaModifiableProp p) override;

        int                                                 popupsCount();

        using CGeometricMovableAnimated::m_realPosition;
        using CGeometricMovableAnimated::m_realSize;

        WP<CLayerShellResource> m_layerSurface;

        // the header providing the enum type cannot be imported here
        int                                     m_interactivity = 0;

        bool                                    m_mapped = false;
        uint32_t                                m_layer  = 0;

        PHLMONITORREF                           m_monitor;

        bool                                    m_noProcess       = false;
        bool                                    m_aboveFullscreen = true;

        UP<Desktop::Rule::CLayerRuleApplicator> m_ruleApplicator;

        PHLLSREF                                m_self;

        CLayerSurfaceAnimationController        m_animationController;

        CBox                                    m_geometry = {0, 0, 0, 0};
        Vector2D                                m_position;
        std::string                             m_namespace = "";
        SP<Desktop::View::CPopup>               m_popupHead;

        pid_t                                   getPID();
        void                                    updateSurfaceScaleTransformDetails();

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

        // fade in/out
        Desktop::Types::CMultiAVarContainer<float, std::underlying_type_t<eLayerAlpha>> m_alpha;

        // For the list lookup
        bool operator==(const CLayerSurface& rhs) const {
            return m_layerSurface == rhs.m_layerSurface && m_monitor == rhs.m_monitor;
        }
    };

    inline bool valid(PHLLS l) {
        return !!l;
    }

    inline bool valid(PHLLSREF l) {
        return !!l;
    }

    inline bool validMapped(const PHLLS& l) {
        if (!valid(l))
            return false;
        return l->aliveAndVisible();
    }

    inline bool validMapped(const PHLLSREF& l) {
        if (!valid(l))
            return false;
        return l->aliveAndVisible();
    }

}
