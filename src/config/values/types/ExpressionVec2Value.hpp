#pragma once

#include "IValue.hpp"

#include "../../../helpers/math/Expression.hpp"

namespace Config::Values {
    struct SExpressionVec2ValueOptions {
        Supplementary::PropRefreshBits refresh           = 0;
        const char*                    deprecationNotice = nullptr;
    };

    class CExpressionVec2Value : public IValue {
      public:
        CExpressionVec2Value(const char* name, const char* description, Math::SExpressionVec2 def = {}, SExpressionVec2ValueOptions&& options = {});

        virtual ~CExpressionVec2Value() = default;

        virtual const std::type_info* underlying() const override;
        virtual void                  commence() override;

        const Math::SExpressionVec2&  defaultVal() const;

      private:
        Math::SExpressionVec2 m_default;
    };
}
