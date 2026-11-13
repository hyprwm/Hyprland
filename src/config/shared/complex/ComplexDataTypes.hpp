#pragma once
#include "ComplexDataType.hpp"
#include "../../../helpers/Color.hpp"

#include <hyprutils/string/VarList2.hpp>

#include <vector>
#include <format>
#include <map>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace Config {

    class CGradientValueData : public IComplexConfigValue {
      public:
        CGradientValueData() = default;
        CGradientValueData(CHyprColor col) {
            m_colors.push_back(col);
            updateColorsOk();
        };
        CGradientValueData(std::vector<CHyprColor>&& cols, float ang) : m_colors(std::move(cols)), m_angle(ang) {
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

        virtual std::string toString() const {
            std::string result;
            for (auto& c : m_colors) {
                result += std::format("{:x} ", c.getAsHex());
            }

            result += std::format("{}deg", sc<int>(m_angle * 180.0 / M_PI));
            return result;
        }
    };

    class CCssGapData : public IComplexConfigValue {
      public:
        CCssGapData() : m_top(0), m_right(0), m_bottom(0), m_left(0) {};
        CCssGapData(double global) : m_top(global), m_right(global), m_bottom(global), m_left(global) {};
        CCssGapData(double vertical, double horizontal) : m_top(vertical), m_right(horizontal), m_bottom(vertical), m_left(horizontal) {};
        CCssGapData(double top, double horizontal, double bottom) : m_top(top), m_right(horizontal), m_bottom(bottom), m_left(horizontal) {};
        CCssGapData(double top, double right, double bottom, double left) : m_top(top), m_right(right), m_bottom(bottom), m_left(left) {};

        /* Css like directions */
        double m_top;
        double m_right;
        double m_bottom;
        double m_left;

        void   parseGapData(Hyprutils::String::CVarList2 varlist) {
            const auto toNumber = [](std::string_view string) -> double {
                const auto VALUE = std::stod(std::string(string));
                if (!std::isfinite(VALUE))
                    throw std::invalid_argument("gap must be finite");
                return VALUE;
            };

            switch (varlist.size()) {
                case 1: {
                    *this = CCssGapData(toNumber(varlist[0]));
                    break;
                }
                case 2: {
                    *this = CCssGapData(toNumber(varlist[0]), toNumber(varlist[1]));
                    break;
                }
                case 3: {
                    *this = CCssGapData(toNumber(varlist[0]), toNumber(varlist[1]), toNumber(varlist[2]));
                    break;
                }
                case 4: {
                    *this = CCssGapData(toNumber(varlist[0]), toNumber(varlist[1]), toNumber(varlist[2]), toNumber(varlist[3]));
                    break;
                }
                default: {
                    LOG(Log::WARN, "Too many arguments provided for gaps.");
                    *this = CCssGapData(toNumber(varlist[0]), toNumber(varlist[1]), toNumber(varlist[2]), toNumber(varlist[3]));
                    break;
                }
            }
        }

        void reset(double global) {
            m_top    = global;
            m_right  = global;
            m_bottom = global;
            m_left   = global;
        }

        virtual eConfigValueDataTypes getDataType() {
            return CVD_TYPE_CSS_VALUE;
        }

        virtual std::string toString() const {
            return std::format("{} {} {} {}", m_top, m_right, m_bottom, m_left);
        }
    };

    class CFontWeightConfigValueData : public IComplexConfigValue {
      public:
        CFontWeightConfigValueData() = default;
        CFontWeightConfigValueData(const char* weight) {
            parseWeight(weight);
        }
        CFontWeightConfigValueData(int64_t def) : m_value(def) {
            ;
        }

        int64_t                       m_value = 400; // default to normal weight

        virtual eConfigValueDataTypes getDataType() {
            return CVD_TYPE_FONT_WEIGHT;
        }

        virtual std::string toString() const {
            return std::format("{}", m_value);
        }

        inline static const auto WEIGHTS = std::map<std::string, int>{
            {"thin", 100},   {"ultralight", 200}, {"light", 300}, {"semilight", 350}, {"book", 380},  {"normal", 400},
            {"medium", 500}, {"semibold", 600},   {"bold", 700},  {"ultrabold", 800}, {"heavy", 900}, {"ultraheavy", 1000},
        };

        void parseWeight(const std::string& strWeight) {
            auto lcWeight{strWeight};
            std::ranges::transform(strWeight, lcWeight.begin(), ::tolower);

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

}
