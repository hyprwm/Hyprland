#pragma once

#include "IValue.hpp"

#include "../../ConfigValue.hpp"

namespace Config::Values {
    class CBoolValue : public IValue {
      public:
        CBoolValue(const char* name, const char* description, Config::BOOL def);

        virtual ~CBoolValue() = default;

        virtual const std::type_info* underlying() const override;
        virtual void                  commence() override;

        Config::BOOL                  value() const;
        Config::BOOL                  defaultVal() const;

      private:
        CConfigValue<Config::BOOL> m_val;
        bool                       m_default = false;
    };
}