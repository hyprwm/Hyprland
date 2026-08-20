#pragma once

#include <cstddef>
#include <string>

namespace IPC::Socket2 {
    class IClient {
      public:
        virtual ~IClient() = default;

        virtual size_t id() const = 0;

      protected:
        IClient() = default;
    };

    class IImplementation {
      public:
        virtual ~IImplementation() = default;

        virtual bool send(std::string&& x) = 0;

      protected:
        IImplementation() = default;
    };
}