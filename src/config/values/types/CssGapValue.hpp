#pragma once

#include "IValue.hpp"

#include "../../ConfigValue.hpp"
#include "../../shared/complex/ComplexDataTypes.hpp"

#include <optional>

namespace Config::Values {
    class CCssGapValue : public IValue {
      public:
        CCssGapValue(const char* name, const char* description, Config::INTEGER def, std::optional<int64_t> min = std::nullopt, std::optional<int64_t> max = std::nullopt);

        virtual ~CCssGapValue() = default;

        virtual const std::type_info* underlying() const override;
        virtual void                  commence() override;

        const Config::CCssGapData&    value() const;
        const Config::CCssGapData&    defaultVal() const;

        const std::optional<int64_t>  m_min, m_max;

      private:
        CConfigValue<Config::IComplexConfigValue> m_val;
        Config::CCssGapData                       m_default;
    };
}