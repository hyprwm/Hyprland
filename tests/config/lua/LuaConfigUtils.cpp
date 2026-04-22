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

using namespace Config;
using namespace Config::Lua;

namespace {
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
