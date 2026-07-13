#include "GeometricMovableAnimated.hpp"

#include <utility>

using namespace Desktop;
using namespace Desktop::View;

Vector2D CGeometricMovableAnimated::position(eGeometricValueType t) const {
    switch (t) {
        case IGeometric::GEOMETRIC_GOAL: return m_realPosition->goal();
        case IGeometric::GEOMETRIC_CURRENT: return m_realPosition->value();
    }

    std::unreachable();
}

Vector2D CGeometricMovableAnimated::size(eGeometricValueType t) const {
    switch (t) {
        case IGeometric::GEOMETRIC_GOAL: return m_realSize->goal();
        case IGeometric::GEOMETRIC_CURRENT: return m_realSize->value();
    }

    std::unreachable();
}

CBox CGeometricMovableAnimated::geometricBox(eGeometricValueType t) const {
    switch (t) {
        case IGeometric::GEOMETRIC_GOAL: return CBox{m_realPosition->goal(), m_realSize->goal()};
        case IGeometric::GEOMETRIC_CURRENT: return CBox{m_realPosition->value(), m_realSize->value()};
    }

    std::unreachable();
}

void CGeometricMovableAnimated::move(const Vector2D& x) {
    *m_realPosition = x;
}

void CGeometricMovableAnimated::resize(const Vector2D& x) {
    *m_realSize = x;
}

void CGeometricMovableAnimated::setBox(const CBox& x) {
    *m_realPosition = x.pos();
    *m_realSize     = x.size();
}

void CGeometricMovableAnimated::finishAnimation() {
    m_realPosition->warp();
    m_realSize->warp();
}
