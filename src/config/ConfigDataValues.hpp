#pragma once
#include "../defines.hpp"
#include "../helpers/varlist/VarList.hpp"
#include <vector>

enum eConfigValueDataTypes : int8_t {
    CVD_TYPE_INVALID   = -1,
    CVD_TYPE_GRADIENT  = 0,
    CVD_TYPE_CSS_VALUE = 1
};

class CCustomConfigValueData {
  public:
    virtual ~CCustomConfigValueData() = default;

    virtual eConfigValueDataTypes getDataType() = 0;

    virtual std::string           toString() = 0;
};

class CGradientValueData : public CCustomConfigValueData {
  public:
    CGradientValueData() = default;
    CGradientValueData(CHyprColor col) {
        m_vColors.push_back(col);
        updateColorsOk();
    };
    virtual ~CGradientValueData() = default;

    virtual eConfigValueDataTypes getDataType() {
        return CVD_TYPE_GRADIENT;
    }

    void reset(CHyprColor col) {
        m_vColors.clear();
        m_vColors.emplace_back(col);
        m_fAngle = 0;
        updateColorsOk();
    }

    void updateColorsOk() {
        m_vColorsOkLabA.clear();
        for (auto& c : m_vColors) {
            const auto OKLAB = c.asOkLab();
            m_vColorsOkLabA.emplace_back(OKLAB.l);
            m_vColorsOkLabA.emplace_back(OKLAB.a);
            m_vColorsOkLabA.emplace_back(OKLAB.b);
            m_vColorsOkLabA.emplace_back(c.a);
        }
    }

    /* Vector containing the colors */
    std::vector<CHyprColor> m_vColors;

    /* Vector containing pure colors for shoving into opengl */
    std::vector<float> m_vColorsOkLabA;

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

    virtual std::string toString() {
        std::string result;
        for (auto& c : m_vColors) {
            result += std::format("{:x} ", c.getAsHex());
        }

        result += std::format("{}deg", (int)(m_fAngle * 180.0 / M_PI));
        return result;
    }
};

class CCssGapData : public CCustomConfigValueData {
  public:
    CCssGapData() : m_top(0), m_right(0), m_bottom(0), m_left(0) {};
    CCssGapData(int64_t global) : m_top(global), m_right(global), m_bottom(global), m_left(global) {};
    CCssGapData(int64_t vertical, int64_t horizontal) : m_top(vertical), m_right(horizontal), m_bottom(vertical), m_left(horizontal) {};
    CCssGapData(int64_t m_top, int64_t horizontal, int64_t m_bottom) : m_top(m_top), m_right(horizontal), m_bottom(m_bottom), m_left(horizontal) {};
    CCssGapData(int64_t m_top, int64_t m_right, int64_t m_bottom, int64_t m_left) : m_top(m_top), m_right(m_right), m_bottom(m_bottom), m_left(m_left) {};

    /* Css like directions */
    int64_t m_top;
    int64_t m_right;
    int64_t m_bottom;
    int64_t m_left;

    void    parseGapData(CVarList varlist) {
        switch (varlist.size()) {
            case 1: {
                *this = CCssGapData(std::stoi(varlist[0]));
                break;
            }
            case 2: {
                *this = CCssGapData(std::stoi(varlist[0]), std::stoi(varlist[1]));
                break;
            }
            case 3: {
                *this = CCssGapData(std::stoi(varlist[0]), std::stoi(varlist[1]), std::stoi(varlist[2]));
                break;
            }
            case 4: {
                *this = CCssGapData(std::stoi(varlist[0]), std::stoi(varlist[1]), std::stoi(varlist[2]), std::stoi(varlist[3]));
                break;
            }
            default: {
                NDebug::log(WARN, "Too many arguments provided for gaps.");
                *this = CCssGapData(std::stoi(varlist[0]), std::stoi(varlist[1]), std::stoi(varlist[2]), std::stoi(varlist[3]));
                break;
            }
        }
    }

    void reset(int64_t global) {
        m_top    = global;
        m_right  = global;
        m_bottom = global;
        m_left   = global;
    }

    virtual eConfigValueDataTypes getDataType() {
        return CVD_TYPE_CSS_VALUE;
    }

    virtual std::string toString() {
        return std::format("{} {} {} {}", m_top, m_right, m_bottom, m_left);
    }
};
