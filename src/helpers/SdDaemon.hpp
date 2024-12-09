#pragma once

namespace NSystemd {
    int sdBooted(void);
    int sdNotify(int unset_environment, const char* state);
}
