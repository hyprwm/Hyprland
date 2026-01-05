#pragma once

#include "../../helpers/math/Math.hpp"
#include "../../helpers/memory/Memory.hpp"

namespace Layout {
    enum eTargetType : uint8_t {
        TARGET_TYPE_WINDOW = 0,
        TARGET_TYPE_GROUP,
    };

    class CSpace;

    class ITarget {
      public:
        virtual ~ITarget() = default;

        virtual eTargetType type() = 0;

        // position is within its space
        virtual void       setPosition(const CBox& box);
        virtual void       assignToSpace(const SP<CSpace>& space);
        virtual SP<CSpace> space() const;

        // general data getters
        virtual bool shouldBeFloated() = 0;

      protected:
        ITarget() = default;

        CBox        m_box;
        SP<CSpace>  m_space;
        WP<ITarget> m_self;
    };
};