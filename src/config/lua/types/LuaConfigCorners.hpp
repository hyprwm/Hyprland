#pragma once

#include "LuaConfigValue.hpp"
#include "../../shared/complex/CornersData.hpp"

#include <optional>

namespace Config::Lua {
    class CLuaConfigCorners : public ILuaConfigValue {
      public:
        CLuaConfigCorners(Config::INTEGER def, std::optional<int64_t> min = std::nullopt, std::optional<int64_t> max = std::nullopt);

        SParseError           parse(lua_State* s) override;
        const std::type_info* underlying() override;
        const void*           data() override;
        std::string           toString() override;
        void                  push(lua_State* s) override;
        void                  reset() override;

        const CCornersData&   parsed() const;

      private:
        Config::INTEGER        m_default = 0;
        CCornersData           m_data;
        std::optional<int64_t> m_min, m_max;
    };
}
