#include "BezierCurve.hpp"

void CBezierCurve::setup(std::vector<Vector2D>* pVec) {
    m_dPoints.clear();

    m_dPoints.emplace_back(Vector2D(0,0));

    for (auto& p : *pVec) {
        m_dPoints.push_back(p);
    }

    m_dPoints.emplace_back(Vector2D(1,1));

    RASSERT(m_dPoints.size() == 4, "CBezierCurve only supports cubic beziers! (points num: %i)", m_dPoints.size());

    // bake 100 points for faster lookups
    // T -> X ( / 100 )
    for (int i = 0; i < 100; ++i) {
        m_aPointsBaked[i] = getXForT((i + 1) / 100.f);
    }
}

float CBezierCurve::getYForT(float t) {
    return 3 * t * pow(1 - t, 2) * m_dPoints[1].y + 3 * pow(t, 2) * (1 - t) * m_dPoints[2].y + pow(t, 3);
}

float CBezierCurve::getXForT(float t) {
    return 3 * t * pow(1 - t, 2) * m_dPoints[1].x + 3 * pow(t, 2) * (1 - t) * m_dPoints[2].x + pow(t, 3);
}

// Todo: this probably can be done better and faster
float CBezierCurve::getYForPoint(float x) {
    // binary search for the range UPDOWN X
    float upperX = 1;
    float lowerX = 0;
    float mid = 0.5;

    while(std::abs(upperX - lowerX) > 0.01f) {
        if (m_aPointsBaked[((int)(mid * 100.f))] > x) {
            upperX = mid;
        } else {
            lowerX = mid;
        }

        mid = (upperX + lowerX) / 2.f;
    }

    // in the name of performance i shall make a hack
    const auto PERCINDELTA = (x - m_aPointsBaked[(int)(100.f * lowerX)]) / (m_aPointsBaked[(int)(100.f * upperX)] - m_aPointsBaked[(int)(100.f * lowerX)]);

    if (std::isnan(PERCINDELTA) || std::isinf(PERCINDELTA)) // can sometimes happen for VERY small x
        return 0.f;

    return getYForT(mid + PERCINDELTA * 0.01f);
}