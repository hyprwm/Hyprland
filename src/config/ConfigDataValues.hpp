#pragma once
#include "../defines.hpp"
#include "../helpers/varlist/VarList.hpp"
#include <vector>

enum eConfigValueDataTypes : int8_t {
    CVD_TYPE_INVALID     = -1,
    CVD_TYPE_GRADIENT    = 0,
    CVD_TYPE_CSS_VALUE   = 1,
    CVD_TYPE_FONT_WEIGHT = 2,
};

class ICustomConfigValueData {
  public:
    virtual ~ICustomConfigValueData() = default;

    virtual eConfigValueDataTypes getDataType() = 0;

    virtual std::string           toString() = 0;
};

class CGradientValueData : public ICustomConfigValueData {
  public:
    CGradientValueData() = default;
    CGradientValueData(CHyprColor col) {
        m_colors.push_back(col);
        updateColorsOk();
    };
    virtual ~CGradientValueData() = default;

    virtual eConfigValueDataTypes getDataType() {
        return CVD_TYPE_GRADIENT;
    }

    void reset(CHyprColor col) {
        m_colors.clear();
        m_colors.emplace_back(col);
        m_angle = 0;
        updateColorsOk();
    }

    void updateColorsOk() {
        m_colorsOkLabA.clear();
        for (auto& c : m_colors) {
            const auto OKLAB = c.asOkLab();
            m_colorsOkLabA.emplace_back(OKLAB.l);
            m_colorsOkLabA.emplace_back(OKLAB.a);
            m_colorsOkLabA.emplace_back(OKLAB.b);
            m_colorsOkLabA.emplace_back(c.a);
        }
    }

    /* Vector containing the colors */
    std::vector<CHyprColor> m_colors;

    /* Vector containing pure colors for shoving into opengl */
    std::vector<float> m_colorsOkLabA;

    /* Float corresponding to the angle (rad) */
    float m_angle = 0;

    //
    bool operator==(const CGradientValueData& other) const {
        if (other.m_colors.size() != m_colors.size() || m_angle != other.m_angle)
            return false;

        for (size_t i = 0; i < m_colors.size(); ++i)
            if (m_colors[i] != other.m_colors[i])
                return false;

        return true;
    }

    virtual std::string toString() {
        std::string result;
        for (auto& c : m_colors) {
            result += std::format("{:x} ", c.getAsHex());
        }

        result += std::format("{}deg", (int)(m_angle * 180.0 / M_PI));
        return result;
    }
};

class CCssGapData : public ICustomConfigValueData {
  public:
    CCssGapData() : m_top(0), m_right(0), m_bottom(0), m_left(0) {};
    CCssGapData(int64_t global) : m_top(global), m_right(global), m_bottom(global), m_left(global) {};
    CCssGapData(int64_t vertical, int64_t horizontal) : m_top(vertical), m_right(horizontal), m_bottom(vertical), m_left(horizontal) {};
    CCssGapData(int64_t top, int64_t horizontal, int64_t bottom) : m_top(top), m_right(horizontal), m_bottom(bottom), m_left(horizontal) {};
    CCssGapData(int64_t top, int64_t right, int64_t bottom, int64_t left) : m_top(top), m_right(right), m_bottom(bottom), m_left(left) {};

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
                Debug::log(WARN, "Too many arguments provided for gaps.");
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

class CFontWeightConfigValueData : public ICustomConfigValueData {
  public:
    CFontWeightConfigValueData() = default;
    CFontWeightConfigValueData(const char* weight) {
        std::string strWeight = weight;
        value                 = parseWeight(strWeight);
    }

    int64_t                       value;

    virtual eConfigValueDataTypes getDataType() {
        return CVD_TYPE_FONT_WEIGHT;
    }

    virtual std::string toString() {
        return std::format("{}", value);
    }

    static int parseWeight(const std::string& weight) {
        auto loWeight{weight};
        transform(weight.begin(), weight.end(), loWeight.begin(), ::tolower);

        // values taken from Pango weight enums
        if (loWeight == "thin")
            return 100;
        if (loWeight == "ultralight")
            return 200;
        if (loWeight == "light")
            return 300;
        if (loWeight == "semilight")
            return 350;
        if (loWeight == "book")
            return 380;
        if (loWeight == "normal")
            return 400;
        if (loWeight == "medium")
            return 500;
        if (loWeight == "semibold")
            return 600;
        if (loWeight == "bold")
            return 700;
        if (loWeight == "ultrabold")
            return 800;
        if (loWeight == "heavy")
            return 900;
        if (loWeight == "ultraheavy")
            return 1000;

        int w_i = std::stoi(weight);
        if (w_i < 100 || w_i > 1000)
            return 400;

        return w_i;
    }
};
