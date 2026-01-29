#pragma once

#include "../ConfigFixSimpleRewriter.hpp"

namespace Config::Supplementary {
    class CFixer12033 : public IConfigFixSimpleRewriter {
      public:
        CFixer12033() = default;

        virtual ~CFixer12033() = default;

        virtual bool        check(const std::string& fileContent);
        virtual std::string run(const std::string& fileContent);
    };
}