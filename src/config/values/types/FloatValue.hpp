#pragma once

#include "IValue.hpp"

#include "../../ConfigValue.hpp"

namespace Config::Values {
    class CFloatValue : public IValue {
      public:
        CFloatValue(const char* name, const char* description, Config::FLOAT def, std::optional<Config::FLOAT> min = std::nullopt, std::optional<Config::FLOAT> max = std::nullopt);

        virtual ~CFloatValue() = default;

        virtual const std::type_info*      underlying() const override;
        virtual void                       commence() override;

        Config::FLOAT                      value() const;
        Config::FLOAT                      defaultVal() const;

        const std::optional<Config::FLOAT> m_min, m_max;

      private:
        CConfigValue<Config::FLOAT> m_val;
        Config::FLOAT               m_default = 0.F;
    };
}