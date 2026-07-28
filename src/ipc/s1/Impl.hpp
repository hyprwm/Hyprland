#pragma once

#include "S1.hpp"

#include <functional>

namespace IPC::Socket1 {
    class IImplementation {
      public:
        using FRequestHandler = std::function<SResponse(std::string&&, pid_t)>;

        virtual ~IImplementation() = default;

        virtual void start(FRequestHandler&& handler) = 0;

      protected:
        IImplementation() = default;
    };
}
