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
    const auto refreshBits = v->refreshBits();
    const auto withRefresh = [refreshBits](UP<ILuaConfigValue> val) -> UP<ILuaConfigValue> {
        val->setRefreshBits(refreshBits);
        return val;
    };

    if (auto p = dc<Config::Values::CIntValue*>(v.get()))
        return withRefresh(makeUnique<CLuaConfigInt>(p->defaultVal()));
    if (auto p = dc<Config::Values::CFloatValue*>(v.get()))
        return withRefresh(makeUnique<CLuaConfigFloat>(p->defaultVal()));
    if (auto p = dc<Config::Values::CBoolValue*>(v.get()))
        return withRefresh(makeUnique<CLuaConfigBool>(p->defaultVal()));
    if (auto p = dc<Config::Values::CStringValue*>(v.get()))
        return withRefresh(makeUnique<CLuaConfigString>(p->defaultVal()));
    if (auto p = dc<Config::Values::CColorValue*>(v.get()))
        return withRefresh(makeUnique<CLuaConfigColor>(p->defaultVal()));
    if (auto p = dc<Config::Values::CVec2Value*>(v.get()))
        return withRefresh(makeUnique<CLuaConfigVec2>(p->defaultVal()));
    if (auto p = dc<Config::Values::CCssGapValue*>(v.get()))
        return withRefresh(makeUnique<CLuaConfigCssGap>(p->defaultVal().m_top));
    if (auto p = dc<Config::Values::CFontWeightValue*>(v.get()))
        return withRefresh(makeUnique<CLuaConfigFontWeight>(p->defaultVal().m_value));
    if (auto p = dc<Config::Values::CGradientValue*>(v.get()))
        return withRefresh(makeUnique<CLuaConfigGradient>(p->defaultVal().m_colors.empty() ? CHyprColor{} : p->defaultVal().m_colors.front()));

    return nullptr;
}
