#include "GeometricStatic.hpp"

using namespace Desktop;
using namespace Desktop::View;

Vector2D CGeometricStatic::position(eGeometricValueType) const {
    return m_box.pos();
}

Vector2D CGeometricStatic::size(eGeometricValueType) const {
    return m_box.size();
}

CBox CGeometricStatic::geometricBox(eGeometricValueType) const {
    return m_box;
}
