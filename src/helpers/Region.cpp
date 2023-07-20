#include "Region.hpp"
#include <wlr/util/box.h>

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

CRegion& CRegion::translate(const Vector2D& vec) {
    pixman_region32_translate(&m_rRegion, vec.x, vec.y);
    return *this;
}

std::vector<pixman_box32_t> CRegion::getRects() const {
    std::vector<pixman_box32_t> result;

    int                         rectsNum = 0;
    const auto                  RECTSARR = pixman_region32_rectangles(&m_rRegion, &rectsNum);

    result.assign(RECTSARR, RECTSARR + rectsNum);

    return result;
}

bool CRegion::empty() {
    return !pixman_region32_not_empty(&m_rRegion);
}
