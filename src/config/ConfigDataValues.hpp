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
    virtual ~ICustomConfigValueData() = default;

    virtual eConfigValueDataTypes getDataType() = 0;

    virtual std::string           toString() = 0;
};

class CGradientValueData : public ICustomConfigValueData {
  public:
    CGradientValueData() = default;
    CGradientValueData(CHyprColor col);
    virtual ~CGradientValueData() = default;

    virtual eConfigValueDataTypes getDataType();

    void                          reset(CHyprColor col);

    void                          updateColorsOk();

    /* Vector containing the colors */
    std::vector<CHyprColor> m_vColors;

    /* Vector containing pure colors for shoving into opengl */
    std::vector<float> m_vColorsOkLabA;

    /* Float corresponding to the angle (rad) */
    float m_fAngle = 0;

    //
    bool                operator==(const CGradientValueData& other) const;

    virtual std::string toString();
};

class CCssGapData : public ICustomConfigValueData {
  public:
    CCssGapData();
    CCssGapData(int64_t global);
    CCssGapData(int64_t vertical, int64_t horizontal);
    CCssGapData(int64_t m_top, int64_t horizontal, int64_t m_bottom);
    CCssGapData(int64_t m_top, int64_t m_right, int64_t m_bottom, int64_t m_left);

    /* Css like directions */
    int64_t                       m_top;
    int64_t                       m_right;
    int64_t                       m_bottom;
    int64_t                       m_left;

    void                          parseGapData(CVarList varlist);

    void                          reset(int64_t global);

    virtual eConfigValueDataTypes getDataType();

    virtual std::string           toString();
};
