#pragma once
#include "../defines.hpp"
#include <vector>

enum eConfigValueDataTypes
{
    CVD_TYPE_INVALID  = -1,
    CVD_TYPE_GRADIENT = 0
};

interface ICustomConfigValueData {
  public:
    virtual ~ICustomConfigValueData() = 0;

    virtual eConfigValueDataTypes getDataType() = 0;
};

class CGradientValueData : public ICustomConfigValueData {
  public:
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

    bool  operator==(const CGradientValueData& other) {
        if (other.m_vColors.size() != m_vColors.size() || m_fAngle != other.m_fAngle)
            return false;

        for (size_t i = 0; i < m_vColors.size(); ++i)
            if (m_vColors[i] != other.m_vColors[i])
                return false;

        return true;
    }
};
