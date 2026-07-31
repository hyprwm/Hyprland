#pragma once

#include "math/Math.hpp"

#include <chrono>
#include <optional>
#include <vector>

namespace Hyprutils::Animation {
    struct SSpringCurve;
}

struct SMeshRenderVertex {
    float x = 0.F, y = 0.F;
    float u = 0.F, v = 0.F;
};

class CDeformableMesh {
  public:
    explicit CDeformableMesh(size_t verticesPerEdge = 8);

    void                           setSize(size_t verticesPerEdge);
    size_t                         size() const;

    void                           reset();
    void                           onPositionUpdate(const CBox& previous, const CBox& current, float intensity, std::optional<Vector2D> grabPoint = std::nullopt);
    void                           advance(const Hyprutils::Animation::SSpringCurve& spring, std::chrono::duration<float> elapsed);

    bool                           stable(float positionEpsilon, float velocityEpsilon) const;
    CBox                           transformedExtents(const CBox& box) const;
    std::vector<SMeshRenderVertex> verticesForBox(const CBox& box, const CBox& outputBox, const Vector2D& textureSize, double displacementScale = 1.0) const;

  private:
    struct SPoint {
        Vector2D displacement;
        Vector2D velocity;
    };

    SPoint&             point(size_t x, size_t y);
    const SPoint&       point(size_t x, size_t y) const;
    Vector2D            restPoint(const CBox& box, size_t x, size_t y) const;
    void                clampDisplacement(double maxDisplacement);

    size_t              m_verticesPerEdge = 8;
    std::vector<SPoint> m_points;
};
