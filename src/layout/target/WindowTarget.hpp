#pragma once

#include "Target.hpp"

#include <string>

namespace Math {
    struct SExpressionVec2;
}

namespace Layout {

    class CWindowTarget : public ITarget {
      public:
        static SP<CWindowTarget> create(PHLWINDOW w);
        virtual ~CWindowTarget() = default;

        virtual eTargetType type();

        using ITarget::setPositionGlobal;
        virtual void                                                setPositionGlobal(const STargetBox& box, uint8_t flags = TARGET_UPDATE_NONE);
        virtual void                                                assignToSpace(const SP<CSpace>& space, std::optional<Vector2D> focalPoint = std::nullopt);
        virtual PHLWINDOW                                           window() const;

        virtual bool                                                floating();
        virtual void                                                setFloating(bool x);
        void                                                        setFloatingInitial(bool x);
        bool                                                        cantLockCursor() const;
        void                                                        setCantLockCursor(bool x);
        virtual std::expected<SGeometryRequested, eGeometryFailure> desiredGeometry();
        virtual std::optional<Vector2D>                             minSize();
        virtual std::optional<Vector2D>                             maxSize();
        virtual void                                                damageEntire();
        virtual void                                                warpPositionSize();
        virtual void                                                onUpdateSpace();

        bool                                                        clampWindowSize(const std::optional<Vector2D> minSize, const std::optional<Vector2D> maxSize);
        std::optional<Vector2D>                                     calculateExpression(const Math::SExpressionVec2& expr);
        void                                                        sendWindowSize(bool force = false);

      private:
        CWindowTarget(PHLWINDOW w);

        std::optional<double> calculateSingleExpr(const std::string& s);
        Vector2D              clampSizeForDesired(const Vector2D& size);

        void                  updatePos(uint8_t flags = TARGET_UPDATE_NONE);

        PHLWINDOWREF          m_window;
        bool                  m_floating       = false;
        bool                  m_cantLockCursor = false;
    };
};
