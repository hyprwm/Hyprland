#pragma once
#include "../defines.hpp"
#include "../helpers/varlist/VarList.hpp"
#include <vector>
#include <map>

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

        result += std::format("{}deg", sc<int>(m_angle * 180.0 / M_PI));
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

    void    parseGapData(CVarList2 varlist) {
        const auto toInt = [](std::string_view string) -> int { return std::stoi(std::string(string)); };

        switch (varlist.size()) {
            case 1: {
                *this = CCssGapData(toInt(varlist[0]));
                break;
            }
            case 2: {
                *this = CCssGapData(toInt(varlist[0]), toInt(varlist[1]));
                break;
            }
            case 3: {
                *this = CCssGapData(toInt(varlist[0]), toInt(varlist[1]), toInt(varlist[2]));
                break;
            }
            case 4: {
                *this = CCssGapData(toInt(varlist[0]), toInt(varlist[1]), toInt(varlist[2]), toInt(varlist[3]));
                break;
            }
            default: {
                Debug::log(WARN, "Too many arguments provided for gaps.");
                *this = CCssGapData(toInt(varlist[0]), toInt(varlist[1]), toInt(varlist[2]), toInt(varlist[3]));
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
        parseWeight(weight);
    }

    int64_t                       m_value = 400; // default to normal weight

    virtual eConfigValueDataTypes getDataType() {
        return CVD_TYPE_FONT_WEIGHT;
    }

    virtual std::string toString() {
        return std::format("{}", m_value);
    }

    void parseWeight(const std::string& strWeight) {
        auto lcWeight{strWeight};
        std::ranges::transform(strWeight, lcWeight.begin(), ::tolower);

        // values taken from Pango weight enums
        const auto WEIGHTS = std::map<std::string, int>{
            {"thin", 100},   {"ultralight", 200}, {"light", 300}, {"semilight", 350}, {"book", 380},  {"normal", 400},
            {"medium", 500}, {"semibold", 600},   {"bold", 700},  {"ultrabold", 800}, {"heavy", 900}, {"ultraheavy", 1000},
        };

        auto weight = WEIGHTS.find(lcWeight);
        if (weight != WEIGHTS.end())
            m_value = weight->second;
        else {
            int w_i = std::stoi(strWeight);
            if (w_i < 100 || w_i > 1000)
                m_value = 400;
        }
    }
};
