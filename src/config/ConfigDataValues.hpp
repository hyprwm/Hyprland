#pragma once
#include "../defines.hpp"
#include <vector>
#include <hyprutils/string/VarList.hpp>

/**
* @enum eConfigValueDataTypes
* @brief Enumerates the possible data types for configuration values.
*/
enum eConfigValueDataTypes {
    CVD_TYPE_INVALID   = -1,
    CVD_TYPE_GRADIENT  = 0,
    CVD_TYPE_CSS_VALUE = 1
};

/**
* @class ICustomConfigValueData
* @brief Abstract base class for custom configuration value data.
*/
class ICustomConfigValueData {
  public:
    /** @brief Virtual destructor. */
    virtual ~ICustomConfigValueData() = 0;

    /**
    * @brief Get the data type of the custom configuration value.
    * @return The data type.
    */
    virtual eConfigValueDataTypes getDataType() = 0;

    /**
    * @brief Convert the custom configuration value to a string.
    * @return The string representation of the value.
    */
    virtual std::string toString() = 0;
};

/**
 * @class CGradientValueData
 * @brief Represents gradient configuration value data.
 */
class CGradientValueData : public ICustomConfigValueData {
  public:

    /** @brief Default constructor. */
    CGradientValueData() {};

    /**
    * @brief Constructor with initial color.
    * @param col Initial color.
    */
    CGradientValueData(CColor col) {
        m_vColors.push_back(col);
    };


    /** @brief Destructor. */
    virtual ~CGradientValueData() {};

    /**
    * @brief Get the data type of the gradient value.
    * @return The data type (CVD_TYPE_GRADIENT).
    */
    virtual eConfigValueDataTypes getDataType() {
        return CVD_TYPE_GRADIENT;
    }

    /**
    * @brief Reset the gradient with a single color.
    * @param col The color to reset the gradient to.
    */
    void reset(CColor col) {
        m_vColors.clear();
        m_vColors.emplace_back(col);
        m_fAngle = 0;
    }

    /** @brief Vector containing the colors */
    std::vector<CColor> m_vColors;

    /** @brief Float representing the angle (in radians). */
    float m_fAngle = 0;

    /**
    * @brief Equality operator for comparing gradient values.
    * @param other The other gradient value to compare with.
    * @return True if equal, false otherwise.
    */
    bool operator==(const CGradientValueData& other) const {
        if (other.m_vColors.size() != m_vColors.size() || m_fAngle != other.m_fAngle)
            return false;

        for (size_t i = 0; i < m_vColors.size(); ++i)
            if (m_vColors[i] != other.m_vColors[i])
                return false;

        return true;
    }

    /**
    * @brief Convert the gradient value to a string.
    * @return The string representation of the gradient value.
    */
    virtual std::string toString() {
        std::string result;
        for (auto& c : m_vColors) {
            result += std::format("{:x} ", c.getAsHex());
        }

        result += std::format("{}deg", (int)(m_fAngle * 180.0 / M_PI));
        return result;
    }
};

/**
 * @class CCssGapData
 * @brief Represents CSS gap configuration value data.
 */
class CCssGapData : public ICustomConfigValueData {
  public:

    /** @brief Default constructor. */
    CCssGapData() : top(0), right(0), bottom(0), left(0) {};

    /**
    * @brief Constructor with global gap size.
    * @param global The global gap size.
    */
    CCssGapData(int64_t global) : top(global), right(global), bottom(global), left(global) {};

    /**
    * @brief Constructor with vertical and horizontal gap sizes.
    * @param vertical The vertical gap size.
    * @param horizontal The horizontal gap size.
    */
    CCssGapData(int64_t vertical, int64_t horizontal) : top(vertical), right(horizontal), bottom(vertical), left(horizontal) {};

    /**
    * @brief Constructor with top, horizontal, and bottom gap sizes.
    * @param top The top gap size.
    * @param horizontal The horizontal gap size.
    * @param bottom The bottom gap size.
    */
    CCssGapData(int64_t top, int64_t horizontal, int64_t bottom) : top(top), right(horizontal), bottom(bottom), left(horizontal) {};

    /**
    * @brief Constructor with top, right, bottom, and left gap sizes.
    * @param top The top gap size.
    * @param right The right gap size.
    * @param bottom The bottom gap size.
    * @param left The left gap size.
    */
    CCssGapData(int64_t top, int64_t right, int64_t bottom, int64_t left) : top(top), right(right), bottom(bottom), left(left) {};

    /* Css like directions */
    int64_t top;
    int64_t right;
    int64_t bottom;
    int64_t left;

    /**
    * @brief Parses gap data from a variable list.
    * @param varlist The variable list to parse.
    */
    void parseGapData(Hyprutils::String::CVarList varlist) {
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

    /**
    * @brief Reset the gaps to a global size.
    * @param global The global gap size.
    */
    void reset(int64_t global) {
        top    = global;
        right  = global;
        bottom = global;
        left   = global;
    }

    /**
    * @brief Get the data type of the CSS gap value.
    * @return The data type (CVD_TYPE_CSS_VALUE).
    */
    virtual eConfigValueDataTypes getDataType() {
        return CVD_TYPE_CSS_VALUE;
    }

    /**
    * @brief Convert the CSS gap value to a string.
    * @return The string representation of the CSS gap value.
    */
    virtual std::string toString() {
        return std::format("{} {} {} {}", top, right, bottom, left);
    }
};
