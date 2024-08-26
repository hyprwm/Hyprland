#include "BezierCurve.hpp"
#include "../debug/Log.hpp"
#include "../macros.hpp"

#include <chrono>
#include <algorithm>

void CBezierCurve::setup(std::vector<Vector2D>* pVec) {
    m_dPoints.clear();

    const auto BEGIN = std::chrono::high_resolution_clock::now();

    m_dPoints.emplace_back(Vector2D(0, 0));

    for (auto const& p : *pVec) {
        m_dPoints.push_back(p);
    }

    m_dPoints.emplace_back(Vector2D(1, 1));

    RASSERT(m_dPoints.size() == 4, "CBezierCurve only supports cubic beziers! (points num: {})", m_dPoints.size());

    // bake BAKEDPOINTS points for faster lookups
    // T -> X ( / BAKEDPOINTS )
    for (int i = 0; i < BAKEDPOINTS; ++i) {
        m_aPointsBaked[i] = Vector2D(getXForT((i + 1) / (float)BAKEDPOINTS), getYForT((i + 1) / (float)BAKEDPOINTS));
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

float CBezierCurve::getYForT(float t) {
    return 3 * t * pow(1 - t, 2) * m_dPoints[1].y + 3 * pow(t, 2) * (1 - t) * m_dPoints[2].y + pow(t, 3);
}

float CBezierCurve::getXForT(float t) {
    return 3 * t * pow(1 - t, 2) * m_dPoints[1].x + 3 * pow(t, 2) * (1 - t) * m_dPoints[2].x + pow(t, 3);
}

// Todo: this probably can be done better and faster
float CBezierCurve::getYForPoint(float x) {
    if (x >= 1.f)
        return 1.f;

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
