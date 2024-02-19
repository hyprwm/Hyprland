#pragma once
#include <pixman.h>
#include <vector>
#include "Vector2D.hpp"
#include "Box.hpp"

struct wlr_box;

class CRegion {
  public:
    /* Create an empty region */
    CRegion();
    /* Create from a reference. Copies, does not own. */
    CRegion(pixman_region32_t* ref);
    /* Create from a box */
    CRegion(double x, double y, double w, double h);
    /* Create from a wlr_box */
    CRegion(wlr_box* box);
    /* Create from a CBox */
    CRegion(const CBox& box);
    /* Create from a pixman_box32_t */
    CRegion(pixman_box32_t* box);

    CRegion(const CRegion&);
    CRegion(CRegion&&);

    ~CRegion();

    CRegion& operator=(CRegion&& other) {
        pixman_region32_copy(&m_rRegion, other.pixman());
        return *this;
    }

    CRegion& operator=(CRegion& other) {
        pixman_region32_copy(&m_rRegion, other.pixman());
        return *this;
    }

    CRegion&                    clear();
    CRegion&                    set(const CRegion& other);
    CRegion&                    add(const CRegion& other);
    CRegion&                    add(double x, double y, double w, double h);
    CRegion&                    add(const CBox& other);
    CRegion&                    subtract(const CRegion& other);
    CRegion&                    intersect(const CRegion& other);
    CRegion&                    intersect(double x, double y, double w, double h);
    CRegion&                    translate(const Vector2D& vec);
    CRegion&                    transform(const wl_output_transform t, double w, double h);
    CRegion&                    invert(pixman_box32_t* box);
    CRegion&                    invert(const CBox& box);
    CRegion&                    scale(float scale);
    CRegion&                    scale(const Vector2D& scale);
    CBox                        getExtents();
    bool                        containsPoint(const Vector2D& vec) const;
    bool                        empty() const;
    Vector2D                    closestPoint(const Vector2D& vec) const;
    CRegion                     copy() const;

    std::vector<pixman_box32_t> getRects() const;

    pixman_region32_t*          pixman() {
                 return &m_rRegion;
    }

  private:
    pixman_region32_t m_rRegion;
};
