#pragma once

#include "ComplexDataType.hpp"

namespace Config {
    class CCornersData : public IComplexConfigValue {
      public:
        CCornersData(int64_t all = 0);
        CCornersData(int64_t topLeft, int64_t topRight, int64_t bottomRight, int64_t bottomLeft);

        int64_t               m_topLeft = 0, m_topRight = 0, m_bottomRight = 0, m_bottomLeft = 0;

        eConfigValueDataTypes getDataType() override;
        bool                  isUniform() const;
        std::string           toString() const override;
        std::string           toJson() const;
    };
}
