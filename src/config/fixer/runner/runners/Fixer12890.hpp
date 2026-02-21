#pragma once

#include "../ConfigFixSimpleRewriter.hpp"

namespace Config::Supplementary {
    class CFixer12890 : public IConfigFixSimpleRewriter {
      public:
        CFixer12890() = default;

        virtual ~CFixer12890() = default;

        virtual bool        check(const std::string& fileContent);
        virtual std::string run(const std::string& fileContent);
    };
}
