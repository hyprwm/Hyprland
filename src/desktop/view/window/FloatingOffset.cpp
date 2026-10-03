#include "FloatingOffset.hpp"

using namespace Desktop::View;

const Hyprutils::Math::Vector2D& CFloatingOffset::value() const {
    return m_value;
}

eFloatingOffsetSource CFloatingOffset::source() const {
    return m_source;
}

void CFloatingOffset::set(const Hyprutils::Math::Vector2D& value, eFloatingOffsetSource source) {
    m_value  = value;
    m_source = source;
}

void CFloatingOffset::clear() {
    set({});
}
