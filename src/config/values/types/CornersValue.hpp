#pragma once

#include "IValue.hpp"
#include "../../ConfigValue.hpp"
#include "../../shared/complex/CornersData.hpp"

#include <optional>

namespace Config::Values {
    struct SCornersValueOptions {
        std::optional<int64_t>         min               = std::nullopt;
        std::optional<int64_t>         max               = std::nullopt;
        Supplementary::PropRefreshBits refresh           = 0;
        const char*                    deprecationNotice = nullptr;
    };

    class CCornersValue : public IValue {
      public:
        CCornersValue(const char* name, const char* description, Config::INTEGER def, SCornersValueOptions&& options = {});

        const std::type_info*        underlying() const override;
        void                         commence() override;

        const CCornersData&          value() const;
        const CCornersData&          defaultVal() const;

        const std::optional<int64_t> m_min, m_max;

      private:
        CConfigValue<IComplexConfigValue> m_val;
        CCornersData                      m_default;
    };
}
