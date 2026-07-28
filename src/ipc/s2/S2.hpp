#pragma once

#include <string>

#include "../../helpers/memory/Memory.hpp"

namespace IPC::Socket2 {
    struct SEvent {
        std::string event;
        std::string data;
    };

    class IImplementation;

    class CSocket2 {
      public:
        CSocket2();
        ~CSocket2() = default;

        void postEvent(SEvent&& event);

      private:
        UP<IImplementation> m_impl;
    };

    UP<CSocket2>& sock();
};