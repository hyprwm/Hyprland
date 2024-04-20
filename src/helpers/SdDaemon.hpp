#pragma once

namespace Systemd {
    int SdBooted(void);
    int SdNotify(int unset_environment, const char* state);
}
