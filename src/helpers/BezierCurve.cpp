#include "BezierCurve.hpp"
#include "../debug/Log.hpp"
#include "../macros.hpp"

#include <chrono>
#include <algorithm>

void CBezierCurve::setup(std::vector<Vector2D>* pVec) {
    const auto BEGIN = std::chrono::high_resolution_clock::now();

    // Avoid reallocations by reserving enough memory upfront
    m_vPoints.resize(pVec->size() + 2);
    m_vPoints[0] = Vector2D(0, 0); // Start point
    size_t index = 1;              // Start after the first element
    for (const auto& vec : *pVec) {
        if (index < m_vPoints.size() - 1) { // Bounds check to ensure safety
            m_vPoints[index] = vec;
            ++index;
        }
    }
    m_vPoints.back() = Vector2D(1, 1); // End point

    RASSERT(m_vPoints.size() == 4, "CBezierCurve only supports cubic beziers! (points num: {})", m_vPoints.size());

    // bake BAKEDPOINTS points for faster lookups
    // T -> X ( / BAKEDPOINTS )
    for (int i = 0; i < BAKEDPOINTS; ++i) {
        float const t     = (i + 1) / (float)BAKEDPOINTS;
        m_aPointsBaked[i] = Vector2D(getXForT(t), getYForT(t));
    }

    const auto ELAPSEDUS  = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - BEGIN).count() / 1000.f;
    const auto POINTSSIZE = m_aPointsBaked.size() * sizeof(m_aPointsBaked[0]) / 1000.f;

    const auto BEGINCALC = std::chrono::high_resolution_clock::now();
    for (int j = 1; j < 10; ++j) {
        float i = j / 10.0f;
        getYForPoint(i);
    }
    const auto ELAPSEDCALCAVG = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - BEGINCALC).count() / 1000.f / 10.f;

    Debug::log(LOG, "Created a bezier curve, baked {} points, mem usage: {:.2f}kB, time to bake: {:.2f}µs. Estimated average calc time: {:.2f}µs.", BAKEDPOINTS, POINTSSIZE,
               ELAPSEDUS, ELAPSEDCALCAVG);
}

float CBezierCurve::getXForT(float const& t) {
    float t2 = t * t;
    float t3 = t2 * t;

    return 3 * t * (1 - t) * (1 - t) * m_vPoints[1].x + 3 * t2 * (1 - t) * m_vPoints[2].x + t3 * m_vPoints[3].x;
}

float CBezierCurve::getYForT(float const& t) {
    float t2 = t * t;
    float t3 = t2 * t;

    return 3 * t * (1 - t) * (1 - t) * m_vPoints[1].y + 3 * t2 * (1 - t) * m_vPoints[2].y + t3 * m_vPoints[3].y;
}

// Todo: this probably can be done better and faster
float CBezierCurve::getYForPoint(float const& x) {
    if (x >= 1.f)
        return 1.f;
    if (x <= 0.f)
        return 0.f;

    int  index = 0;
    bool below = true;
    for (int step = (BAKEDPOINTS + 1) / 2; step > 0; step /= 2) {
        if (below)
            index += step;
        else
            index -= step;

        below = m_aPointsBaked[index].x < x;
    }

    int lowerIndex = index - (!below || index == BAKEDPOINTS - 1);

    // in the name of performance i shall make a hack
    const auto LOWERPOINT = &m_aPointsBaked[lowerIndex];
    const auto UPPERPOINT = &m_aPointsBaked[lowerIndex + 1];

    const auto PERCINDELTA = (x - LOWERPOINT->x) / (UPPERPOINT->x - LOWERPOINT->x);

    if (std::isnan(PERCINDELTA) || std::isinf(PERCINDELTA)) // can sometimes happen for VERY small x
        return 0.f;

    return LOWERPOINT->y + (UPPERPOINT->y - LOWERPOINT->y) * PERCINDELTA;
}
