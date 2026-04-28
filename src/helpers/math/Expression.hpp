#pragma once

#include "../memory/Memory.hpp"
#include <expected>
#include <string>
#include <optional>
#include <string_view>

namespace mu {
    class Parser;
};

namespace Math {
    struct SExpressionVec2 {
        std::string x;
        std::string y;

        bool        empty() const;
        std::string toString() const;
    };

    std::expected<SExpressionVec2, std::string> parseExpressionVec2(std::string_view raw);

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
