#pragma once

#include "IValue.hpp"

#include "../../ConfigValue.hpp"
#include "../../shared/complex/ComplexDataTypes.hpp"

namespace Config::Values {
    struct SGradientValueOptions {
        Supplementary::PropRefreshBits refresh = 0;
    };

    class CGradientValue : public IValue {
      public:
        CGradientValue(const char* name, const char* description, CHyprColor def, SGradientValueOptions&& options = {});

        virtual ~CGradientValue() = default;

        virtual const std::type_info*     underlying() const override;
        virtual void                      commence() override;

        const Config::CGradientValueData& value() const;
        const Config::CGradientValueData& defaultVal() const;

      private:
        CConfigValue<Config::IComplexConfigValue> m_val;
        Config::CGradientValueData                m_default;
    };
}
