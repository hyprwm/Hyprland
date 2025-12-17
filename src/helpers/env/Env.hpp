#pragma once

#include <string>

namespace Env {
    bool envEnabled(const std::string& env);
    bool isTrace();
}
