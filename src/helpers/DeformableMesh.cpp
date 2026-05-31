#include "DeformableMesh.hpp"

#include <hyprutils/animation/Spring.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace Hyprutils::Animation;

CDeformableMesh::CDeformableMesh(size_t verticesPerEdge) {
    setSize(verticesPerEdge);
}

void CDeformableMesh::setSize(size_t verticesPerEdge) {
    verticesPerEdge = std::clamp(verticesPerEdge, static_cast<size_t>(2), static_cast<size_t>(32));
    if (m_verticesPerEdge == verticesPerEdge && m_points.size() == verticesPerEdge * verticesPerEdge)
        return;

    m_verticesPerEdge = verticesPerEdge;
    m_points.assign(m_verticesPerEdge * m_verticesPerEdge, {});
}

size_t CDeformableMesh::size() const {
    return m_verticesPerEdge;
}

void CDeformableMesh::reset() {
    for (auto& p : m_points) {
        p.displacement = {};
        p.velocity     = {};
    }
}

void CDeformableMesh::onPositionUpdate(const CBox& previous, const CBox& current, float intensity, std::optional<Vector2D> grabPoint) {
    if (previous == current || previous.w <= 0.F || previous.h <= 0.F || current.w <= 0.F || current.h <= 0.F)
        return;

    intensity                      = std::clamp(intensity, 0.F, 2.F);
    const double   MAXDISPLACEMENT = std::clamp(std::min(current.w, current.h) * 0.25, 16.0, 160.0);

    const double   PREVLEFT   = previous.x;
    const double   PREVRIGHT  = previous.x + previous.w;
    const double   PREVTOP    = previous.y;
    const double   PREVBOTTOM = previous.y + previous.h;

    const double   CURRLEFT   = current.x;
    const double   CURRRIGHT  = current.x + current.w;
    const double   CURRTOP    = current.y;
    const double   CURRBOTTOM = current.y + current.h;

    const Vector2D MOVEDELTA = current.pos() - previous.pos();
    const double   LEFTDELTA = CURRLEFT - PREVLEFT, RIGHTDELTA = CURRRIGHT - PREVRIGHT;
    const double   TOPDELTA = CURRTOP - PREVTOP, BOTTOMDELTA = CURRBOTTOM - PREVBOTTOM;

    if (grabPoint)
        *grabPoint = Vector2D{std::clamp(grabPoint->x, 0.0, 1.0), std::clamp(grabPoint->y, 0.0, 1.0)};

    const double GRABMAXDISTANCE = grabPoint ? std::max(std::hypot(std::max(grabPoint->x, 1.0 - grabPoint->x), std::max(grabPoint->y, 1.0 - grabPoint->y)), 0.001) : 1.0;

    for (size_t y = 0; y < m_verticesPerEdge; ++y) {
        const double V = m_verticesPerEdge == 1 ? 0.0 : static_cast<double>(y) / static_cast<double>(m_verticesPerEdge - 1);
        for (size_t x = 0; x < m_verticesPerEdge; ++x) {
            const double U = m_verticesPerEdge == 1 ? 0.0 : static_cast<double>(x) / static_cast<double>(m_verticesPerEdge - 1);

            const double MOVEWEIGHTX = 0.45 + 0.55 * std::sin(V * std::numbers::pi);
            const double MOVEWEIGHTY = 0.45 + 0.55 * std::sin(U * std::numbers::pi);

            Vector2D     impulse;
            impulse.x = -MOVEDELTA.x * MOVEWEIGHTX;
            impulse.y = -MOVEDELTA.y * MOVEWEIGHTY;

            impulse.x += -(LEFTDELTA * (1.0 - U) + RIGHTDELTA * U);
            impulse.y += -(TOPDELTA * (1.0 - V) + BOTTOMDELTA * V);

            if (grabPoint) {
                const double DISTANCE   = std::hypot(U - grabPoint->x, V - grabPoint->y);
                const double GRABWEIGHT = std::clamp(DISTANCE / GRABMAXDISTANCE, 0.0, 1.0);

                impulse = impulse * GRABWEIGHT;
            }

            point(x, y).displacement += impulse * intensity;
        }
    }

    clampDisplacement(MAXDISPLACEMENT);
}

void CDeformableMesh::advance(const SSpringCurve& spring, std::chrono::duration<float> elapsed) {
    for (auto& p : m_points) {
        float valueX = 1.F + static_cast<float>(p.displacement.x);
        float valueY = 1.F + static_cast<float>(p.displacement.y);
        float velX   = static_cast<float>(p.velocity.x);
        float velY   = static_cast<float>(p.velocity.y);

        advanceSpring(valueX, velX, spring, elapsed);
        advanceSpring(valueY, velY, spring, elapsed);

        p.displacement.x = valueX - 1.F;
        p.displacement.y = valueY - 1.F;
        p.velocity.x     = velX;
        p.velocity.y     = velY;
    }
}

bool CDeformableMesh::stable(float positionEpsilon, float velocityEpsilon) const {
    for (auto const& p : m_points) {
        if (std::abs(p.displacement.x) > positionEpsilon || std::abs(p.displacement.y) > positionEpsilon || std::abs(p.velocity.x) > velocityEpsilon ||
            std::abs(p.velocity.y) > velocityEpsilon)
            return false;
    }

    return true;
}

CBox CDeformableMesh::transformedExtents(const CBox& box) const {
    if (m_points.empty())
        return box;

    double minX = box.x, minY = box.y, maxX = box.x + box.w, maxY = box.y + box.h;
    for (size_t y = 0; y < m_verticesPerEdge; ++y) {
        for (size_t x = 0; x < m_verticesPerEdge; ++x) {
            const Vector2D POS = restPoint(box, x, y) + point(x, y).displacement;
            minX               = std::min(minX, POS.x);
            minY               = std::min(minY, POS.y);
            maxX               = std::max(maxX, POS.x);
            maxY               = std::max(maxY, POS.y);
        }
    }

    return {minX, minY, maxX - minX, maxY - minY};
}

std::vector<SMeshRenderVertex> CDeformableMesh::verticesForBox(const CBox& box, const CBox& outputBox, const Vector2D& textureSize, double displacementScale) const {
    std::vector<SMeshRenderVertex> vertices;
    if (m_verticesPerEdge < 2 || outputBox.w <= 0.F || outputBox.h <= 0.F || textureSize.x <= 0.F || textureSize.y <= 0.F)
        return vertices;

    vertices.reserve((m_verticesPerEdge - 1) * (m_verticesPerEdge - 1) * 6);

    const auto makeVertex = [&](size_t x, size_t y) -> SMeshRenderVertex {
        const Vector2D REST = restPoint(box, x, y);
        const Vector2D POS  = REST + point(x, y).displacement * displacementScale;
        return {
            .x = static_cast<float>((POS.x - outputBox.x) / outputBox.w),
            .y = static_cast<float>((POS.y - outputBox.y) / outputBox.h),
            .u = static_cast<float>(REST.x / textureSize.x),
            .v = static_cast<float>(REST.y / textureSize.y),
        };
    };

    for (size_t y = 0; y < m_verticesPerEdge - 1; ++y) {
        for (size_t x = 0; x < m_verticesPerEdge - 1; ++x) {
            vertices.emplace_back(makeVertex(x, y));
            vertices.emplace_back(makeVertex(x, y + 1));
            vertices.emplace_back(makeVertex(x + 1, y));
            vertices.emplace_back(makeVertex(x + 1, y));
            vertices.emplace_back(makeVertex(x, y + 1));
            vertices.emplace_back(makeVertex(x + 1, y + 1));
        }
    }

    return vertices;
}

CDeformableMesh::SPoint& CDeformableMesh::point(size_t x, size_t y) {
    return m_points[y * m_verticesPerEdge + x];
}

const CDeformableMesh::SPoint& CDeformableMesh::point(size_t x, size_t y) const {
    return m_points[y * m_verticesPerEdge + x];
}

Vector2D CDeformableMesh::restPoint(const CBox& box, size_t x, size_t y) const {
    const double U = m_verticesPerEdge == 1 ? 0.0 : static_cast<double>(x) / static_cast<double>(m_verticesPerEdge - 1);
    const double V = m_verticesPerEdge == 1 ? 0.0 : static_cast<double>(y) / static_cast<double>(m_verticesPerEdge - 1);
    return {box.x + box.w * U, box.y + box.h * V};
}

void CDeformableMesh::clampDisplacement(double maxDisplacement) {
    maxDisplacement = std::max(maxDisplacement, 0.0);

    for (auto& p : m_points) {
        const double LENGTH = std::hypot(p.displacement.x, p.displacement.y);
        if (LENGTH <= maxDisplacement || LENGTH <= 0.0)
            continue;

        p.displacement = p.displacement * (maxDisplacement / LENGTH);
    }
}
