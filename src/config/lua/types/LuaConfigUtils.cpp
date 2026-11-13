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
#include "LuaConfigExpressionVec2.hpp"
#include "../../values/types/IntValue.hpp"
#include "../../values/types/FloatValue.hpp"
#include "../../values/types/BoolValue.hpp"
#include "../../values/types/StringValue.hpp"
#include "../../values/types/ColorValue.hpp"
#include "../../values/types/Vec2Value.hpp"
#include "../../values/types/CssGapValue.hpp"
#include "../../values/types/FontWeightValue.hpp"
#include "../../values/types/GradientValue.hpp"
#include "../../values/types/ExpressionVec2Value.hpp"

using namespace Config;
using namespace Config::Lua;

SP<ILuaConfigValue> Lua::fromGenericValue(SP<Config::Values::IValue> v) {
    const auto refreshBits = v->refreshBits();
    const auto deprecated  = v->deprecationNotice();
    const auto populate    = [refreshBits, deprecated](SP<ILuaConfigValue> val) -> SP<ILuaConfigValue> {
        val->setRefreshBits(refreshBits);
        if (deprecated)
            val->setDeprecated(*deprecated);
        return val;
    };

    if (auto p = dc<Config::Values::CIntValue*>(v.get()))
        return populate(makeShared<CLuaConfigInt>(p->defaultVal(), p->m_min, p->m_max, p->m_map));
    if (auto p = dc<Config::Values::CFloatValue*>(v.get()))
        return populate(makeShared<CLuaConfigFloat>(p->defaultVal(), p->m_min, p->m_max));
    if (auto p = dc<Config::Values::CBoolValue*>(v.get()))
        return populate(makeShared<CLuaConfigBool>(p->defaultVal()));
    if (auto p = dc<Config::Values::CStringValue*>(v.get()))
        return populate(makeShared<CLuaConfigString>(p->defaultVal(), p->validator()));
    if (auto p = dc<Config::Values::CColorValue*>(v.get()))
        return populate(makeShared<CLuaConfigColor>(p->defaultVal()));
    if (auto p = dc<Config::Values::CVec2Value*>(v.get()))
        return populate(makeShared<CLuaConfigVec2>(p->defaultVal(), p->validator()));
    if (auto p = dc<Config::Values::CCssGapValue*>(v.get()))
        return populate(makeShared<CLuaConfigCssGap>(p->defaultVal().m_top, p->m_min, p->m_max));
    if (auto p = dc<Config::Values::CFontWeightValue*>(v.get()))
        return populate(makeShared<CLuaConfigFontWeight>(p->defaultVal().m_value));
    if (auto p = dc<Config::Values::CGradientValue*>(v.get()))
        return populate(makeShared<CLuaConfigGradient>(p->defaultVal().m_colors.empty() ? CHyprColor{} : p->defaultVal().m_colors.front()));
    if (auto p = dc<Config::Values::CExpressionVec2Value*>(v.get()))
        return populate(makeShared<CLuaConfigExpressionVec2>(p->defaultVal()));

    return nullptr;
}
