#include "Region.hpp"
extern "C" {
#include <wlr/util/box.h>
#include <wlr/util/region.h>
}

CRegion::CRegion() {
    pixman_region32_init(&m_rRegion);
}

CRegion::CRegion(pixman_region32_t* ref) {
    pixman_region32_init(&m_rRegion);
    pixman_region32_copy(&m_rRegion, ref);
}

CRegion::CRegion(double x, double y, double w, double h) {
    pixman_region32_init_rect(&m_rRegion, x, y, w, h);
}

CRegion::CRegion(wlr_box* box) {
    pixman_region32_init_rect(&m_rRegion, box->x, box->y, box->width, box->height);
}

CRegion::CRegion(const CBox& box) {
    pixman_region32_init_rect(&m_rRegion, box.x, box.y, box.w, box.h);
}

CRegion::CRegion(pixman_box32_t* box) {
    pixman_region32_init_rect(&m_rRegion, box->x1, box->y1, box->x2 - box->x1, box->y2 - box->y1);
}

CRegion::CRegion(const CRegion& other) {
    pixman_region32_init(&m_rRegion);
    pixman_region32_copy(&m_rRegion, const_cast<CRegion*>(&other)->pixman());
}

CRegion::CRegion(CRegion&& other) {
    pixman_region32_init(&m_rRegion);
    pixman_region32_copy(&m_rRegion, other.pixman());
}

CRegion::~CRegion() {
    pixman_region32_fini(&m_rRegion);
}

CRegion& CRegion::clear() {
    pixman_region32_clear(&m_rRegion);
    return *this;
}

CRegion& CRegion::set(const CRegion& other) {
    pixman_region32_copy(&m_rRegion, const_cast<CRegion*>(&other)->pixman());
    return *this;
}

CRegion& CRegion::add(const CRegion& other) {
    pixman_region32_union(&m_rRegion, &m_rRegion, const_cast<CRegion*>(&other)->pixman());
    return *this;
}

CRegion& CRegion::add(double x, double y, double w, double h) {
    pixman_region32_union_rect(&m_rRegion, &m_rRegion, x, y, w, h);
    return *this;
}

CRegion& CRegion::add(const CBox& other) {
    pixman_region32_union_rect(&m_rRegion, &m_rRegion, other.x, other.y, other.w, other.h);
    return *this;
}

CRegion& CRegion::subtract(const CRegion& other) {
    pixman_region32_subtract(&m_rRegion, &m_rRegion, const_cast<CRegion*>(&other)->pixman());
    return *this;
}

CRegion& CRegion::intersect(const CRegion& other) {
    pixman_region32_intersect(&m_rRegion, &m_rRegion, const_cast<CRegion*>(&other)->pixman());
    return *this;
}

CRegion& CRegion::intersect(double x, double y, double w, double h) {
    pixman_region32_intersect_rect(&m_rRegion, &m_rRegion, x, y, w, h);
    return *this;
}

CRegion& CRegion::invert(pixman_box32_t* box) {
    pixman_region32_inverse(&m_rRegion, &m_rRegion, box);
    return *this;
}

CRegion& CRegion::invert(const CBox& box) {
    pixman_box32 pixmanBox = {box.x, box.y, box.w + box.x, box.h + box.y};
    return this->invert(&pixmanBox);
}

CRegion& CRegion::translate(const Vector2D& vec) {
    pixman_region32_translate(&m_rRegion, vec.x, vec.y);
    return *this;
}

CRegion& CRegion::transform(const wl_output_transform t, double w, double h) {
    wlr_region_transform(&m_rRegion, &m_rRegion, t, w, h);
    return *this;
}

CRegion CRegion::copy() const {
    return CRegion(*this);
}

CRegion& CRegion::scale(float scale) {
    wlr_region_scale(&m_rRegion, &m_rRegion, scale);
    return *this;
}

CRegion& CRegion::scale(const Vector2D& scale) {
    wlr_region_scale_xy(&m_rRegion, &m_rRegion, scale.x, scale.y);
    return *this;
}

std::vector<pixman_box32_t> CRegion::getRects() const {
    std::vector<pixman_box32_t> result;

    int                         rectsNum = 0;
    const auto                  RECTSARR = pixman_region32_rectangles(&m_rRegion, &rectsNum);

    result.assign(RECTSARR, RECTSARR + rectsNum);

    return result;
}

CBox CRegion::getExtents() {
    pixman_box32_t* box = pixman_region32_extents(&m_rRegion);
    return {box->x1, box->y1, box->x2 - box->x1, box->y2 - box->y1};
}

bool CRegion::containsPoint(const Vector2D& vec) const {
    return pixman_region32_contains_point(&m_rRegion, vec.x, vec.y, nullptr);
}

bool CRegion::empty() const {
    return !pixman_region32_not_empty(&m_rRegion);
}

Vector2D CRegion::closestPoint(const Vector2D& vec) const {
    double   bestDist = __FLT_MAX__;
    Vector2D leader   = vec;

    for (auto& box : getRects()) {
        double x = 0, y = 0;

        if (vec.x >= box.x2)
            x = box.x2 - 1;
        else if (vec.x < box.x1)
            x = box.x1;
        else
            x = vec.x;

        if (vec.y >= box.y2)
            y = box.y2 - 1;
        else if (vec.y < box.y1)
            y = box.y1;
        else
            y = vec.y;

        double distance = sqrt(pow(x, 2) + pow(y, 2));
        if (distance < bestDist) {
            bestDist = distance;
            leader   = {x, y};
        }
    }

    return leader;
}