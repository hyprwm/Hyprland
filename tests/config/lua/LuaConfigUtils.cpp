#include <config/lua/types/LuaConfigUtils.hpp>

#include <config/lua/types/LuaConfigBool.hpp>
#include <config/lua/types/LuaConfigColor.hpp>
#include <config/lua/types/LuaConfigCssGap.hpp>
#include <config/lua/types/LuaConfigFloat.hpp>
#include <config/lua/types/LuaConfigFontWeight.hpp>
#include <config/lua/types/LuaConfigGradient.hpp>
#include <config/lua/types/LuaConfigInt.hpp>
#include <config/lua/types/LuaConfigString.hpp>
#include <config/lua/types/LuaConfigVec2.hpp>

#include <config/values/types/BoolValue.hpp>
#include <config/values/types/ColorValue.hpp>
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

TEST(ConfigLuaUtils, fromGenericValueCopiesRefreshBits) {
    const auto REFRESH = Config::Supplementary::REFRESH_LAYOUTS | Config::Supplementary::REFRESH_WINDOW_STATES;

    auto       out = fromGenericValue(makeShared<Values::CIntValue>("a", "", 1, Values::SIntValueOptions{.refresh = REFRESH}));
    ASSERT_NE(out.get(), nullptr);
    EXPECT_EQ(out->refreshBits(), REFRESH);
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
