#include "ReservedArea.hpp"

using namespace Desktop;

// fuck me. Writing this at 11pm, and I have an in-class test tomorrow.
// I am failing that bitch

CReservedArea::CReservedArea(const Vector2D& tl, const Vector2D& br) : m_initialTopLeft(tl), m_initialBottomRight(br) {
    calculate();
}

CReservedArea::CReservedArea(double top, double right, double bottom, double left) : m_initialTopLeft(left, top), m_initialBottomRight(right, bottom) {
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

#ifdef HL_UNIT_TESTS

#include <gtest/gtest.h>

TEST(Desktop, reservedArea) {
    CReservedArea a{{20, 30}, {40, 50}};
    CBox          box = {1000, 1000, 1000, 1000};
    a.applyip(box);

    EXPECT_EQ(box.x, 1020);
    EXPECT_EQ(box.y, 1030);
    EXPECT_EQ(box.w, 1000 - 20 - 40);
    EXPECT_EQ(box.h, 1000 - 30 - 50);

    box = a.apply(CBox{1000, 1000, 1000, 1000});

    EXPECT_EQ(box.x, 1020);
    EXPECT_EQ(box.y, 1030);
    EXPECT_EQ(box.w, 1000 - 20 - 40);
    EXPECT_EQ(box.h, 1000 - 30 - 50);

    a.addType(RESERVED_DYNAMIC_TYPE_LS, {10, 20}, {30, 40});

    box = a.apply(CBox{1000, 1000, 1000, 1000});

    EXPECT_EQ(box.x, 1000 + 20 + 10);
    EXPECT_EQ(box.y, 1000 + 30 + 20);
    EXPECT_EQ(box.w, 1000 - 20 - 40 - 10 - 30);
    EXPECT_EQ(box.h, 1000 - 30 - 50 - 20 - 40);
}

#endif
