#pragma once

#include "IValue.hpp"

#include "../../ConfigValue.hpp"

#include <optional>
#include <functional>
#include <expected>

namespace Config::Values {
    class CVec2Value : public IValue {
      public:
        CVec2Value(const char* name, const char* description, Config::VEC2 def,
                   std::optional<std::function<std::expected<void, std::string>(const Config::VEC2&)>>&& validator = std::nullopt);

        virtual ~CVec2Value() = default;

        virtual const std::type_info* underlying() const override;
        virtual void                  commence() override;

        Config::VEC2                  value() const;
        Config::VEC2                  defaultVal() const;

      private:
        CConfigValue<Config::VEC2>                                                          m_val;
        std::optional<std::function<std::expected<void, std::string>(const Config::VEC2&)>> m_validator;
        Config::VEC2                                                                        m_default = {};
    };
}