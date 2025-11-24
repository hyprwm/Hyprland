#pragma once

#include "../../helpers/math/Math.hpp"
#include <array>

namespace Desktop {
    enum eReservedDynamicType : uint8_t {
        RESERVED_DYNAMIC_TYPE_LS = 0,
        RESERVED_DYNAMIC_TYPE_ERROR_BAR,

        RESERVED_DYNAMIC_TYPE_END,
    };

    class CReservedArea {
      public:
        CReservedArea() = default;
        CReservedArea(const Vector2D& tl, const Vector2D& br);
        CReservedArea(double top, double right, double bottom, double left);
        CReservedArea(const CBox& parent, const CBox& child);
        ~CReservedArea() = default;

        CBox   apply(const CBox& other) const;
        void   applyip(CBox& other) const;

        void   resetType(eReservedDynamicType);
        void   addType(eReservedDynamicType, const Vector2D& topLeft, const Vector2D& bottomRight);
        void   addType(eReservedDynamicType, const CReservedArea& area);

        double left() const;
        double right() const;
        double top() const;
        double bottom() const;

        bool   operator==(const CReservedArea& other) const;

      private:
        void     calculate();

        Vector2D m_topLeft, m_bottomRight;
        Vector2D m_initialTopLeft, m_initialBottomRight;

        struct SDynamicData {
            Vector2D topLeft, bottomRight;
        };

        std::array<SDynamicData, RESERVED_DYNAMIC_TYPE_END> m_dynamicReserved;
    };
};