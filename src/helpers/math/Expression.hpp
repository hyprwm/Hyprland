#pragma once

#include "../memory/Memory.hpp"
#include <string>
#include <optional>

namespace mu {
    class Parser;
};

namespace Math {
    class CExpression {
      public:
        CExpression();
        ~CExpression() = default;

        CExpression(const CExpression&) = delete;
        CExpression(CExpression&)       = delete;
        CExpression(CExpression&&)      = delete;

        void                  addVariable(const std::string& name, double val);

        std::optional<double> compute(const std::string& expr);

      private:
        UP<mu::Parser> m_parser;
    };
};