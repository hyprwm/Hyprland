#include "GeometricMovableStatic.hpp"

using namespace Desktop;
using namespace Desktop::View;

Vector2D CGeometricMovableStatic::position(eGeometricValueType) const {
    return m_box.pos();
}

Vector2D CGeometricMovableStatic::size(eGeometricValueType) const {
    return m_box.size();
}

CBox CGeometricMovableStatic::geometricBox(eGeometricValueType) const {
    return m_box;
}

void CGeometricMovableStatic::move(const Vector2D& x) {
    m_box.x = x.x;
    m_box.y = x.y;
}

void CGeometricMovableStatic::resize(const Vector2D& x) {
    m_box.w = x.x;
    m_box.h = x.y;
}

void CGeometricMovableStatic::setBox(const CBox& x) {
    m_box = x;
}
