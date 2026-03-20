#pragma once

#include "IValue.hpp"

#include "../../ConfigValue.hpp"

#include <optional>
#include <unordered_map>
#include <string>

namespace Config::Values {
    class CIntValue : public IValue {
      public:
        CIntValue(const char* name, const char* description, Config::INTEGER def, std::optional<Config::INTEGER> min = std::nullopt,
                  std::optional<Config::INTEGER> max = std::nullopt, std::optional<std::unordered_map<std::string, Config::INTEGER>> map = std::nullopt);

        virtual ~CIntValue() = default;

        virtual const std::type_info*                                         underlying() const override;
        virtual void                                                          commence() override;

        Config::INTEGER                                                       value() const;
        Config::INTEGER                                                       defaultVal() const;

        const std::optional<Config::INTEGER>                                  m_min, m_max;
        const std::optional<std::unordered_map<std::string, Config::INTEGER>> m_map;

      private:
        CConfigValue<Config::INTEGER> m_val;
        Config::INTEGER               m_default = 0;
    };
}