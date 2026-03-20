#pragma once

#include "IValue.hpp"

#include "../../ConfigValue.hpp"

namespace Config::Values {
    class CColorValue : public IValue {
      public:
        CColorValue(const char* name, const char* description, Config::INTEGER def);

        virtual ~CColorValue() = default;

        virtual const std::type_info* underlying() const override;
        virtual void                  commence() override;

        Config::INTEGER               value() const;
        Config::INTEGER               defaultVal() const;

      private:
        CConfigValue<Config::INTEGER> m_val;
        Config::INTEGER               m_default = 0;
    };
}