#pragma once

#include <hyprutils/math/Vector2D.hpp>
#include <cstdint>

namespace Desktop::View {
    enum class eFloatingOffsetSource : uint8_t {
        WINDOW,
        WORKSPACE,
    };

    class CFloatingOffset {
      public:
        const Hyprutils::Math::Vector2D& value() const;
        eFloatingOffsetSource            source() const;
        void                             set(const Hyprutils::Math::Vector2D& value, eFloatingOffsetSource source = eFloatingOffsetSource::WINDOW);
        void                             clear();

      private:
        Hyprutils::Math::Vector2D m_value;
        eFloatingOffsetSource     m_source = eFloatingOffsetSource::WINDOW;
    };
}
