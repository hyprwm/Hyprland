#pragma once

#include "S1.hpp"

namespace IPC::Socket1 {
    void registerBuiltinCommands(CSocket1& socket);
    void refreshState();
}
