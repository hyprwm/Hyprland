#include <config/lua/types/LuaConfigUtils.hpp>

#include <config/lua/types/LuaConfigBool.hpp>
#include <config/lua/types/LuaConfigColor.hpp>
#include <config/lua/types/LuaConfigCorners.hpp>
#include <config/lua/types/LuaConfigCssGap.hpp>
#include <config/lua/types/LuaConfigFloat.hpp>
#include <config/lua/types/LuaConfigFontWeight.hpp>
#include <config/lua/types/LuaConfigGradient.hpp>
#include <config/lua/types/LuaConfigInt.hpp>
#include <config/lua/types/LuaConfigString.hpp>
#include <config/lua/types/LuaConfigVec2.hpp>

#include <config/values/types/BoolValue.hpp>
#include <config/values/types/ColorValue.hpp>
#include <config/values/types/CornersValue.hpp>
#include <config/values/types/CssGapValue.hpp>
#include <config/values/types/FloatValue.hpp>
#include <config/values/types/FontWeightValue.hpp>
#include <config/values/types/GradientValue.hpp>
#include <config/values/types/IntValue.hpp>
#include <config/values/types/StringValue.hpp>
#include <config/values/types/Vec2Value.hpp>

#include <gtest/gtest.h>

extern "C" {
#include <lauxlib.h>
}

using namespace Config;
using namespace Config::Lua;

namespace {
    class CLuaState {
      public:
        CLuaState() : m_lua(luaL_newstate()) {
            ;
        }

        ~CLuaState() {
            if (m_lua)
                lua_close(m_lua);
        }

        lua_State* get() const {
            return m_lua;
        }

      private:
        lua_State* m_lua = nullptr;
    };

    class CUnsupportedValue : public Values::IValue {
      public:
        CUnsupportedValue() : Values::IValue(0) {
            m_name        = "unsupported";
            m_description = "unsupported";
        }

        const std::type_info* underlying() const override {
            return &typeid(void);
        }

        void commence() override {
            ;
        }
    };
}

TEST(ConfigLuaUtils, fromGenericValueMapsSupportedTypes) {
    {
        auto out = fromGenericValue(makeShared<Values::CIntValue>("a", "", 1));
        ASSERT_NE(out.get(), nullptr);
        EXPECT_NE(dynamic_cast<CLuaConfigInt*>(out.get()), nullptr);
    }
    {
        auto out = fromGenericValue(makeShared<Values::CFloatValue>("a", "", 1.F));
        ASSERT_NE(out.get(), nullptr);
        EXPECT_NE(dynamic_cast<CLuaConfigFloat*>(out.get()), nullptr);
    }
    {
        auto out = fromGenericValue(makeShared<Values::CBoolValue>("a", "", true));
        ASSERT_NE(out.get(), nullptr);
        EXPECT_NE(dynamic_cast<CLuaConfigBool*>(out.get()), nullptr);
    }
    {
        auto out = fromGenericValue(makeShared<Values::CStringValue>("a", "", "x"));
        ASSERT_NE(out.get(), nullptr);
        EXPECT_NE(dynamic_cast<CLuaConfigString*>(out.get()), nullptr);
    }
    {
        auto out = fromGenericValue(makeShared<Values::CColorValue>("a", "", 0xFF000000));
        ASSERT_NE(out.get(), nullptr);
        EXPECT_NE(dynamic_cast<CLuaConfigColor*>(out.get()), nullptr);
    }
    {
        auto out = fromGenericValue(makeShared<Values::CVec2Value>("a", "", VEC2{1, 2}));
        ASSERT_NE(out.get(), nullptr);
        EXPECT_NE(dynamic_cast<CLuaConfigVec2*>(out.get()), nullptr);
    }
    {
        auto out = fromGenericValue(makeShared<Values::CCssGapValue>("a", "", 5));
        ASSERT_NE(out.get(), nullptr);
        EXPECT_NE(dynamic_cast<CLuaConfigCssGap*>(out.get()), nullptr);
    }
    {
        auto out = fromGenericValue(makeShared<Values::CFontWeightValue>("a", "", 500));
        ASSERT_NE(out.get(), nullptr);
        EXPECT_NE(dynamic_cast<CLuaConfigFontWeight*>(out.get()), nullptr);
    }
    {
        auto out = fromGenericValue(makeShared<Values::CGradientValue>("a", "", CHyprColor(0xFF112233)));
        ASSERT_NE(out.get(), nullptr);
        EXPECT_NE(dynamic_cast<CLuaConfigGradient*>(out.get()), nullptr);
    }
}

TEST(ConfigLuaUtils, fromGenericValueReturnsNullForUnsupportedType) {
    auto out = fromGenericValue(makeShared<CUnsupportedValue>());
    EXPECT_EQ(out.get(), nullptr);
}

TEST(ConfigLuaUtils, cornersDispatchUsesTypeAndCopiesArbitraryBounds) {
    CLuaState  state;
    const auto lua     = state.get();
    const auto refresh = Supplementary::REFRESH_WINDOW_STATES | Supplementary::REFRESH_BLUR_FB;
    const auto source  = makeShared<Values::CCornersValue>("arbitrary:corners", "", -10, Values::SCornersValueOptions{.min = -20, .max = 200, .refresh = refresh});
    const auto out     = fromGenericValue(source);
    ASSERT_NE(out.get(), nullptr);
    const auto value = dynamic_cast<CLuaConfigCorners*>(out.get());
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(out->refreshBits(), refresh);
    EXPECT_EQ(*out->underlying(), typeid(CCornersData));

    for (const auto bound : {-20, 200}) {
        lua_pushinteger(lua, bound);
        ASSERT_EQ(value->parse(lua).errorCode, PARSE_ERROR_OK);
        EXPECT_TRUE(value->parsed().isUniform());
        EXPECT_EQ(value->parsed().m_topLeft, bound);
        EXPECT_EQ(lua_gettop(lua), 1);
        lua_pop(lua, 1);
    }

    ASSERT_EQ(luaL_dostring(lua, "return {top_left = -20, top_right = 200, bottom_right = -10, bottom_left = 150}"), LUA_OK);
    ASSERT_EQ(value->parse(lua).errorCode, PARSE_ERROR_OK);
    lua_pop(lua, 1);
    EXPECT_EQ(value->toString(), "-20 200 -10 150");

    for (const auto bound : {-21, 201}) {
        lua_pushinteger(lua, bound);
        EXPECT_EQ(value->parse(lua).errorCode, PARSE_ERROR_OUT_OF_RANGE);
        EXPECT_EQ(lua_gettop(lua), 1);
        EXPECT_EQ(value->toString(), "-20 200 -10 150");
        lua_pop(lua, 1);

        for (const auto field : {"top_left", "top_right", "bottom_right", "bottom_left"}) {
            SCOPED_TRACE(field);
            ASSERT_EQ(luaL_dostring(lua, std::format("return {{{} = {}}}", field, bound).c_str()), LUA_OK);
            EXPECT_EQ(value->parse(lua).errorCode, PARSE_ERROR_OUT_OF_RANGE);
            EXPECT_EQ(lua_gettop(lua), 1);
            EXPECT_EQ(value->toString(), "-20 200 -10 150");
            lua_pop(lua, 1);
        }
    }

    value->reset();
    value->push(lua);
    EXPECT_TRUE(lua_isinteger(lua, -1));
    EXPECT_EQ(lua_tointeger(lua, -1), -10);
}

TEST(ConfigLuaUtils, fromGenericValueCopiesRefreshBits) {
    const auto REFRESH = Config::Supplementary::REFRESH_LAYOUTS | Config::Supplementary::REFRESH_WINDOW_STATES;

    auto       out = fromGenericValue(makeShared<Values::CIntValue>("a", "", 1, Values::SIntValueOptions{.refresh = REFRESH}));
    ASSERT_NE(out.get(), nullptr);
    EXPECT_EQ(out->refreshBits(), REFRESH);
}

TEST(ConfigValues, valueOptionsCopyDeprecationNotice) {
    constexpr const char* NOTICE = "use another option";

    const auto            checkNotice = [NOTICE](const Values::IValue& value) {
        const auto notice = value.deprecationNotice();
        ASSERT_TRUE(notice.has_value());
        EXPECT_STREQ(*notice, NOTICE);
    };

    checkNotice(*makeShared<Values::CBoolValue>("a", "", false, Values::SBoolValueOptions{.deprecationNotice = NOTICE}));
    checkNotice(*makeShared<Values::CColorValue>("a", "", 0, Values::SColorValueOptions{.deprecationNotice = NOTICE}));
    checkNotice(*makeShared<Values::CCornersValue>("a", "", 0, Values::SCornersValueOptions{.deprecationNotice = NOTICE}));
    checkNotice(*makeShared<Values::CCssGapValue>("a", "", 0, Values::SCssGapValueOptions{.deprecationNotice = NOTICE}));
    checkNotice(*makeShared<Values::CFloatValue>("a", "", 0.F, Values::SFloatValueOptions{.deprecationNotice = NOTICE}));
    checkNotice(*makeShared<Values::CFontWeightValue>("a", "", 400, Values::SFontWeightValueOptions{.deprecationNotice = NOTICE}));
    checkNotice(*makeShared<Values::CGradientValue>("a", "", CHyprColor(0), Values::SGradientValueOptions{.deprecationNotice = NOTICE}));
    checkNotice(*makeShared<Values::CIntValue>("a", "", 0, Values::SIntValueOptions{.deprecationNotice = NOTICE}));
    checkNotice(*makeShared<Values::CStringValue>("a", "", "", Values::SStringValueOptions{.deprecationNotice = NOTICE}));
    checkNotice(*makeShared<Values::CVec2Value>("a", "", VEC2{}, Values::SVec2ValueOptions{.deprecationNotice = NOTICE}));

    const auto defaultValue = makeShared<Values::CBoolValue>("a", "", false);
    EXPECT_FALSE(defaultValue->deprecationNotice().has_value());
}

TEST(ConfigLuaUtils, fromGenericValueCopiesValidationMetadata) {
    CLuaState  state;
    const auto lua = state.get();

    {
        auto out = fromGenericValue(
            makeShared<Values::CIntValue>("a", "", 1, Values::SIntValueOptions{.min = 0, .max = 2, .map = std::unordered_map<std::string, INTEGER>{{"mapped", 2}}}));
        auto value = dynamic_cast<CLuaConfigInt*>(out.get());
        ASSERT_NE(value, nullptr);

        lua_pushstring(lua, "mapped");
        EXPECT_EQ(value->parse(lua).errorCode, PARSE_ERROR_OK);
        lua_pop(lua, 1);

        lua_pushinteger(lua, 3);
        EXPECT_EQ(value->parse(lua).errorCode, PARSE_ERROR_OUT_OF_RANGE);
        lua_pop(lua, 1);
    }

    {
        auto out   = fromGenericValue(makeShared<Values::CFloatValue>("a", "", 0.F, Values::SFloatValueOptions{.min = -1.F, .max = 1.F}));
        auto value = dynamic_cast<CLuaConfigFloat*>(out.get());
        ASSERT_NE(value, nullptr);

        lua_pushnumber(lua, -2.F);
        EXPECT_EQ(value->parse(lua).errorCode, PARSE_ERROR_OUT_OF_RANGE);
        lua_pop(lua, 1);
    }

    {
        auto out = fromGenericValue(
            makeShared<Values::CStringValue>("a", "", "valid", Values::SStringValueOptions{.validator = [](const STRING& value) -> std::expected<void, std::string> {
                                                 if (value == "valid")
                                                     return {};
                                                 return std::unexpected("invalid value");
                                             }}));
        auto value = dynamic_cast<CLuaConfigString*>(out.get());
        ASSERT_NE(value, nullptr);

        lua_pushstring(lua, "invalid");
        EXPECT_EQ(value->parse(lua).errorCode, PARSE_ERROR_BAD_VALUE);
        lua_pop(lua, 1);
    }

    {
        auto out =
            fromGenericValue(makeShared<Values::CVec2Value>("a", "", VEC2{}, Values::SVec2ValueOptions{.validator = [](const VEC2& value) -> std::expected<void, std::string> {
                                                                if (value.x >= 0 && value.y >= 0)
                                                                    return {};
                                                                return std::unexpected("negative component");
                                                            }}));
        auto value = dynamic_cast<CLuaConfigVec2*>(out.get());
        ASSERT_NE(value, nullptr);

        lua_createtable(lua, 2, 0);
        lua_pushnumber(lua, -1);
        lua_rawseti(lua, -2, 1);
        lua_pushnumber(lua, 0);
        lua_rawseti(lua, -2, 2);
        EXPECT_EQ(value->parse(lua).errorCode, PARSE_ERROR_BAD_VALUE);
        lua_pop(lua, 1);
    }

    {
        auto out   = fromGenericValue(makeShared<Values::CCssGapValue>("a", "", 1, Values::SCssGapValueOptions{.min = 0, .max = 2}));
        auto value = dynamic_cast<CLuaConfigCssGap*>(out.get());
        ASSERT_NE(value, nullptr);

        lua_pushinteger(lua, 3);
        EXPECT_EQ(value->parse(lua).errorCode, PARSE_ERROR_OUT_OF_RANGE);
        lua_pop(lua, 1);
    }
}

TEST(ConfigLuaUtils, typedAccessorsReadStoredValues) {
    CLuaConfigBool   boolFalse(false);
    CLuaConfigBool   boolTrue(true);
    CLuaConfigInt    intValue(2);
    CLuaConfigFloat  floatValue(1.25F);
    CLuaConfigVec2   vecValue({3, 4});
    CLuaConfigString stringValue("value");

    EXPECT_EQ(boolFalse.asInt(), 0);
    EXPECT_EQ(boolTrue.asInt(), 1);
    EXPECT_EQ(intValue.asInt(), 2);
    EXPECT_FLOAT_EQ(floatValue.asFloat(), 1.25F);

    const auto vec = vecValue.asVec2();
    EXPECT_EQ(vec.x, 3);
    EXPECT_EQ(vec.y, 4);
    EXPECT_EQ(stringValue.asString(), "value");
}
