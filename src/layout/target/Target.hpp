#pragma once

#include "../../helpers/math/Math.hpp"
#include "../../helpers/memory/Memory.hpp"
#include "../../desktop/Workspace.hpp"

#include <expected>
#include <cstdint>

namespace Layout {
    enum eTargetType : uint8_t {
        TARGET_TYPE_WINDOW = 0,
        TARGET_TYPE_GROUP,
    };

    enum eGeometryFailure : uint8_t {
        GEOMETRY_NO_DESIRED      = 0,
        GEOMETRY_INVALID_DESIRED = 1,
    };

    class CSpace;

    struct SGeometryRequested {
        Vector2D                size;
        std::optional<Vector2D> pos;
    };

    class ITarget {
      public:
        virtual ~ITarget() = default;

        virtual eTargetType type() = 0;

        // position is within its space
        virtual void         setPositionGlobal(const CBox& box);
        virtual CBox         position() const;
        virtual void         assignToSpace(const SP<CSpace>& space, std::optional<Vector2D> focalPoint = std::nullopt);
        virtual void         setSpaceGhost(const SP<CSpace>& space);
        virtual SP<CSpace>   space() const;
        virtual PHLWORKSPACE workspace() const;
        virtual PHLWINDOW    window() const = 0;
        virtual void         recalc();
        virtual bool         wasTiling() const;
        virtual void         setWasTiling(bool x);

        virtual void         rememberFloatingSize(const Vector2D& size);
        virtual Vector2D     lastFloatingSize() const;

        virtual void         setPseudo(bool x);
        virtual bool         isPseudo() const;
        virtual void         setPseudoSize(const Vector2D& size);
        virtual Vector2D     pseudoSize();
        virtual void         swap(SP<ITarget> b);

        //
        virtual bool                                                floating()                              = 0;
        virtual void                                                setFloating(bool x)                     = 0;
        virtual std::expected<SGeometryRequested, eGeometryFailure> desiredGeometry()                       = 0;
        virtual eFullscreenMode                                     fullscreenMode()                        = 0;
        virtual void                                                setFullscreenMode(eFullscreenMode mode) = 0;
        virtual std::optional<Vector2D>                             minSize()                               = 0;
        virtual std::optional<Vector2D>                             maxSize()                               = 0;
        virtual void                                                damageEntire()                          = 0;
        virtual void                                                warpPositionSize()                      = 0;
        virtual void                                                onUpdateSpace()                         = 0;

      protected:
        ITarget() = default;

        CBox        m_box;
        SP<CSpace>  m_space;
        WP<ITarget> m_self;
        Vector2D    m_floatingSize;
        bool        m_pseudo     = false;
        bool        m_ghostSpace = false; // ghost space means a target belongs to a space, but isn't sent to the layout
        Vector2D    m_pseudoSize = {1280, 720};
        bool        m_wasTiling  = false;
    };
};