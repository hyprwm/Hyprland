#include "ReservedArea.hpp"
#include "../../macros.hpp"

using namespace Desktop;

// fuck me. Writing this at 11pm, and I have an in-class test tomorrow.
// I am failing that bitch

CReservedArea::CReservedArea(const Vector2D& tl, const Vector2D& br) : m_initialTopLeft(tl), m_initialBottomRight(br) {
    calculate();
}

CReservedArea::CReservedArea(double top, double right, double bottom, double left) : m_initialTopLeft(left, top), m_initialBottomRight(right, bottom) {
    calculate();
}

CReservedArea::CReservedArea(const CBox& parent, const CBox& child) {
    ASSERT(!parent.empty() && !child.empty());

    ASSERT(parent.containsPoint(child.pos() + Vector2D{0.0001, 0.0001}));
    ASSERT(parent.containsPoint(child.pos() + child.size() - Vector2D{0.0001, 0.0001}));

    m_initialTopLeft     = child.pos() - parent.pos();
    m_initialBottomRight = (parent.pos() + parent.size()) - (child.pos() + child.size());

    calculate();
}

void CReservedArea::calculate() {
    m_bottomRight = m_initialBottomRight;
    m_topLeft     = m_initialTopLeft;

    for (const auto& e : m_dynamicReserved) {
        m_bottomRight += e.bottomRight;
        m_topLeft += e.topLeft;
    }
}

CBox CReservedArea::apply(const CBox& other) const {
    auto c = other.copy();
    c.x += m_topLeft.x;
    c.y += m_topLeft.y;
    c.w -= m_topLeft.x + m_bottomRight.x;
    c.h -= m_topLeft.y + m_bottomRight.y;
    return c;
}

void CReservedArea::applyip(CBox& other) const {
    other.x += m_topLeft.x;
    other.y += m_topLeft.y;
    other.w -= m_topLeft.x + m_bottomRight.x;
    other.h -= m_topLeft.y + m_bottomRight.y;
}

bool CReservedArea::operator==(const CReservedArea& other) const {
    return other.m_bottomRight == m_bottomRight && other.m_topLeft == m_topLeft;
}

double CReservedArea::left() const {
    return m_topLeft.x;
}

double CReservedArea::right() const {
    return m_bottomRight.x;
}

double CReservedArea::top() const {
    return m_topLeft.y;
}

double CReservedArea::bottom() const {
    return m_bottomRight.y;
}

void CReservedArea::resetType(eReservedDynamicType t) {
    m_dynamicReserved[t] = {};
    calculate();
}

void CReservedArea::addType(eReservedDynamicType t, const Vector2D& topLeft, const Vector2D& bottomRight) {
    auto& ref = m_dynamicReserved[t];
    ref.topLeft += topLeft;
    ref.bottomRight += bottomRight;
    calculate();
}

void CReservedArea::addType(eReservedDynamicType t, const CReservedArea& area) {
    addType(t, {area.left(), area.top()}, {area.right(), area.bottom()});
}
