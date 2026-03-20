#include "LuaConfigUtils.hpp"
#include "LuaConfigInt.hpp"
#include "LuaConfigFloat.hpp"
#include "LuaConfigBool.hpp"
#include "LuaConfigString.hpp"
#include "LuaConfigColor.hpp"
#include "LuaConfigVec2.hpp"
#include "LuaConfigCssGap.hpp"
#include "LuaConfigFontWeight.hpp"
#include "LuaConfigGradient.hpp"
#include "../../values/types/IntValue.hpp"
#include "../../values/types/FloatValue.hpp"
#include "../../values/types/BoolValue.hpp"
#include "../../values/types/StringValue.hpp"
#include "../../values/types/ColorValue.hpp"
#include "../../values/types/Vec2Value.hpp"
#include "../../values/types/CssGapValue.hpp"
#include "../../values/types/FontWeightValue.hpp"
#include "../../values/types/GradientValue.hpp"

using namespace Config;
using namespace Config::Lua;

UP<ILuaConfigValue> Lua::fromGenericValue(SP<Config::Values::IValue> v) {
    if (auto p = dc<Config::Values::CIntValue*>(v.get()))
        return makeUnique<CLuaConfigInt>(p->defaultVal());
    if (auto p = dc<Config::Values::CFloatValue*>(v.get()))
        return makeUnique<CLuaConfigFloat>(p->defaultVal());
    if (auto p = dc<Config::Values::CBoolValue*>(v.get()))
        return makeUnique<CLuaConfigBool>(p->defaultVal());
    if (auto p = dc<Config::Values::CStringValue*>(v.get()))
        return makeUnique<CLuaConfigString>(p->defaultVal());
    if (auto p = dc<Config::Values::CColorValue*>(v.get()))
        return makeUnique<CLuaConfigColor>(p->defaultVal());
    if (auto p = dc<Config::Values::CVec2Value*>(v.get()))
        return makeUnique<CLuaConfigVec2>(p->defaultVal());
    if (auto p = dc<Config::Values::CCssGapValue*>(v.get()))
        return makeUnique<CLuaConfigCssGap>(p->defaultVal().m_top);
    if (auto p = dc<Config::Values::CFontWeightValue*>(v.get()))
        return makeUnique<CLuaConfigFontWeight>(p->defaultVal().m_value);
    if (auto p = dc<Config::Values::CGradientValue*>(v.get()))
        return makeUnique<CLuaConfigGradient>(p->defaultVal().m_colors.empty() ? CHyprColor{} : p->defaultVal().m_colors.front());

    return nullptr;
}
