#pragma once

#include <string>
#include <vector>

#include "../../../helpers/memory/Memory.hpp"

namespace Config::Supplementary {
    class IConfigFixRunner {
      protected:
        IConfigFixRunner() = default;

      public:
        virtual ~IConfigFixRunner() = default;

        virtual bool        check(const std::string& fileContent) = 0;
        virtual std::string run(const std::string& fileContent)   = 0;
    };

    inline std::vector<UP<IConfigFixRunner>> fixRunners;
};
