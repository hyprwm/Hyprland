#pragma once

#include <wlr/util/box.h>
#include "Vector2D.hpp"
#include "../SharedDefs.hpp"

class CBox {
  public:
    CBox(double x_, double y_, double w_, double h_) {
        x = x_;
        y = y_;
        w = w_;
        h = h_;
    }

    CBox() {
        w = 0;
        h = 0;
    }

    CBox(const wlr_box& box) {
        x = box.x;
        y = box.y;
        w = box.width;
        h = box.height;
    }

    CBox(const double d) {
        x = d;
        y = d;
        w = d;
        h = d;
    }

    wlr_box  wlr();
    wlr_box* pWlr();

    CBox&    applyFromWlr();
    CBox&    scale(double scale);
    CBox&    scaleFromCenter(double scale);
    CBox&    scale(const Vector2D& scale);
    CBox&    translate(const Vector2D& vec);
    CBox&    round();
    CBox&    transform(const wl_output_transform t, double w, double h);
    CBox&    addExtents(const SWindowDecorationExtents& e);

    Vector2D middle() const;

    bool     containsPoint(const Vector2D& vec) const;
    bool     empty() const;

    CBox&    operator+(const Vector2D& vec) {
        translate(vec);
        return *this;
    }

    CBox& operator+(const SWindowDecorationExtents& e) {
        addExtents(e);
        return *this;
    }

    CBox& operator*(const double s) {
        scale(s);
        return *this;
    }

    CBox& operator*(const Vector2D& s) {
        scale(s);
        return *this;
    }

    double x = 0, y = 0;
    union {
        double w;
        double width;
    };
    union {
        double h;
        double height;
    };

  private:
    CBox    roundInternal();

    wlr_box m_bWlrBox;
};