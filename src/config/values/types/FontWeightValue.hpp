#pragma once

#include "IValue.hpp"

#include "../../ConfigValue.hpp"
#include "../../shared/complex/ComplexDataTypes.hpp"

namespace Config::Values {
    class CFontWeightValue : public IValue {
      public:
        CFontWeightValue(const char* name, const char* description, Config::INTEGER def = 400);

        virtual ~CFontWeightValue() = default;

        virtual const std::type_info*             underlying() const override;
        virtual void                              commence() override;

        const Config::CFontWeightConfigValueData& value() const;
        const Config::CFontWeightConfigValueData& defaultVal() const;

      private:
        CConfigValue<Config::IComplexConfigValue> m_val;
        Config::CFontWeightConfigValueData        m_default;
    };
}