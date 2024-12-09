#pragma once
#include "../defines.hpp"
#include "../helpers/varlist/VarList.hpp"
#include <vector>

enum eConfigValueDataTypes : int8_t {
    CVD_TYPE_INVALID   = -1,
    CVD_TYPE_GRADIENT  = 0,
    CVD_TYPE_CSS_VALUE = 1
};

class ICustomConfigValueData {
  public:
    virtual ~ICustomConfigValueData() = 0;

    virtual eConfigValueDataTypes getDataType() = 0;

    virtual std::string           toString() = 0;
};

class CGradientValueData : public ICustomConfigValueData {
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

class CCssGapData : public ICustomConfigValueData {
  public:
    CCssGapData() : top(0), right(0), bottom(0), left(0) {};
    CCssGapData(int64_t global) : top(global), right(global), bottom(global), left(global) {};
    CCssGapData(int64_t vertical, int64_t horizontal) : top(vertical), right(horizontal), bottom(vertical), left(horizontal) {};
    CCssGapData(int64_t top, int64_t horizontal, int64_t bottom) : top(top), right(horizontal), bottom(bottom), left(horizontal) {};
    CCssGapData(int64_t top, int64_t right, int64_t bottom, int64_t left) : top(top), right(right), bottom(bottom), left(left) {};

    /* Css like directions */
    int64_t top;
    int64_t right;
    int64_t bottom;
    int64_t left;

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
                Debug::log(WARN, "Too many arguments provided for gaps.");
                *this = CCssGapData(std::stoi(varlist[0]), std::stoi(varlist[1]), std::stoi(varlist[2]), std::stoi(varlist[3]));
                break;
            }
        }
    }

    void reset(int64_t global) {
        top    = global;
        right  = global;
        bottom = global;
        left   = global;
    }

    virtual eConfigValueDataTypes getDataType() {
        return CVD_TYPE_CSS_VALUE;
    }

    virtual std::string toString() {
        return std::format("{} {} {} {}", top, right, bottom, left);
    }
};
