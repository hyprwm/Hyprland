#pragma once
#include "../defines.hpp"
#include <vector>

enum eConfigValueDataTypes {
    CVD_TYPE_INVALID      = -1,
    CVD_TYPE_GRADIENT     = 0,
    CVD_TYPE_CORNER_RADII = 1
};

class ICustomConfigValueData {
  public:
    virtual ~ICustomConfigValueData() = 0;

    virtual eConfigValueDataTypes getDataType() = 0;
};

class CGradientValueData : public ICustomConfigValueData {
  public:
    CGradientValueData(){};
    CGradientValueData(CColor col) {
        m_vColors.push_back(col);
    };
    virtual ~CGradientValueData(){};

    virtual eConfigValueDataTypes getDataType() {
        return CVD_TYPE_GRADIENT;
    }

    void reset(CColor col) {
        m_vColors.clear();
        m_vColors.emplace_back(col);
        m_fAngle = 0;
    }

    /* Vector containing the colors */
    std::vector<CColor> m_vColors;

    /* Float corresponding to the angle (rad) */
    float m_fAngle = 0;

    //
    bool operator==(const CGradientValueData& other) const {
        if (other.m_vColors.size() != m_vColors.size() || m_fAngle != other.m_fAngle)
            return false;

        for (size_t i = 0; i < m_vColors.size(); ++i)
            if (m_vColors[i] != other.m_vColors[i])
                return false;

        return true;
    }
};

// This class is probably going to be refactored once hyprlang is used
class CCornerRadiiData : public ICustomConfigValueData {
  public:
    CCornerRadiiData() : CCornerRadiiData(0) {}

    CCornerRadiiData(int radius) : CCornerRadiiData(radius, radius, radius, radius) {}

    CCornerRadiiData(int topL, int topR, int bottomR, int bottomL) : topLeft(topL), topRight(topR), bottomRight(bottomR), bottomLeft(bottomL) {}

    virtual ~CCornerRadiiData() {}

    virtual eConfigValueDataTypes getDataType() {
        return CVD_TYPE_CORNER_RADII;
    }

    void reset(int radius) {
        topLeft = topRight = bottomRight = bottomLeft = radius;
    }

    CCornerRadiiData operator+(int a) const {
        return CCornerRadiiData(topLeft + a, topRight + a, bottomRight + a, bottomLeft + a);
    }

    CCornerRadiiData operator-(int a) const {
        return CCornerRadiiData(topLeft - a, topRight - a, bottomRight - a, bottomLeft - a);
    }

    CCornerRadiiData operator*(int a) const {
        return CCornerRadiiData(topLeft * a, topRight * a, bottomRight * a, bottomLeft * a);
    }

    CCornerRadiiData operator/(int a) const {
        return CCornerRadiiData(topLeft / a, topRight / a, bottomRight / a, bottomLeft / a);
    }

    bool operator==(int a) const {
        return topLeft == a && topRight == a && bottomRight == a && bottomLeft == a;
    }

    bool operator!=(int a) const {
        return topLeft != a && topRight != a && bottomRight != a && bottomLeft != a;
    }

    CCornerRadiiData& operator+=(int a) {
        topLeft += a;
        topRight += a;
        bottomRight += a;
        bottomLeft += a;
        return *this;
    }

    CCornerRadiiData& operator-=(int a) {
        topLeft -= a;
        topRight -= a;
        bottomRight -= a;
        bottomLeft -= a;
        return *this;
    }

    int topLeft;
    int topRight;
    int bottomRight;
    int bottomLeft;
};
