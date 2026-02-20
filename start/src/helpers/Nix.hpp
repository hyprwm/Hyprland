#pragma once

#include <expected>
#include <string>

namespace Nix {
    std::expected<void, std::string> nixEnvironmentOk();
    bool                             shouldUseNixGL();
};