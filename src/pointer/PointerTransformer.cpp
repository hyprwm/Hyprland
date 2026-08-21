#include "PointerTransformer.hpp"

#include <utility>

using namespace Pointer;

CPointerTransformer::CPointerTransformer(std::function<Vector2D(Vector2D)> transform) : m_transform(std::move(transform)) {
    ;
}

Vector2D CPointerTransformer::transform(Vector2D pos) const {
    return m_transform ? m_transform(pos) : pos;
}
