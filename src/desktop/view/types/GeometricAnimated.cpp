#include "GeometricAnimated.hpp"

#include <utility>

using namespace Desktop;
using namespace Desktop::View;

Vector2D CGeometricAnimated::position(eGeometricValueType t) const {
    switch (t) {
        case IGeometric::GEOMETRIC_GOAL: return m_realPosition->goal();
        case IGeometric::GEOMETRIC_CURRENT: return m_realPosition->value();
    }

    std::unreachable();
}

Vector2D CGeometricAnimated::size(eGeometricValueType t) const {
    switch (t) {
        case IGeometric::GEOMETRIC_GOAL: return m_realSize->goal();
        case IGeometric::GEOMETRIC_CURRENT: return m_realSize->value();
    }

    std::unreachable();
}

CBox CGeometricAnimated::geometricBox(eGeometricValueType t) const {
    switch (t) {
        case IGeometric::GEOMETRIC_GOAL: return CBox{m_realPosition->goal(), m_realSize->goal()};
        case IGeometric::GEOMETRIC_CURRENT: return CBox{m_realPosition->value(), m_realSize->value()};
    }

    std::unreachable();
}

void CGeometricAnimated::finishAnimation() {
    m_realPosition->warp();
    m_realSize->warp();
}
