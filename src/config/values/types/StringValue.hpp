#pragma once

#include "IValue.hpp"

#include "../../ConfigValue.hpp"

#include <functional>
#include <expected>

namespace Config::Values {
    struct SStringValueOptions {
        std::function<std::expected<void, std::string>(const Config::STRING&)> validator = {};
        Supplementary::PropRefreshBits                                         refresh   = 0;
    };

    class CStringValue : public IValue {
      public:
        CStringValue(const char* name, const char* description, Config::STRING def, SStringValueOptions&& options = {});

        virtual ~CStringValue() = default;

        virtual const std::type_info* underlying() const override;
        virtual void                  commence() override;

        Config::STRING                value() const;
        Config::STRING                defaultVal() const;

      private:
        CConfigValue<Config::STRING>                                        m_val;
        std::function<std::expected<void, std::string>(const std::string&)> m_validator;
        Config::STRING                                                      m_default = "[[EMPTY]]";
    };
}
