#include "SdDaemon.hpp"

#include <memory>

#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <sys/socket.h>
#include <sys/un.h>
#include <cstdlib>
#include <cstring>

namespace Systemd {
    int SdBooted() {
        if (!faccessat(AT_FDCWD, "/run/systemd/system/", F_OK, AT_SYMLINK_NOFOLLOW))
            return true;

        if (errno == ENOENT)
            return false;

        return -errno;
    }

    int SdNotify(int unsetEnvironment, const char* state) {
        int fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
        if (fd < 0)
            return -errno;

        constexpr char envVar[] = "NOTIFY_SOCKET";

        auto           cleanup = [unsetEnvironment, envVar](const int* fd) {
            if (unsetEnvironment)
                unsetenv(envVar);
            close(*fd);
        };
        std::unique_ptr<int, decltype(cleanup)> fdCleaup(&fd, cleanup);

        const char*                             addr = getenv(envVar);
        if (!addr)
            return 0;

        // address length must be at most this; see man 7 unix
        size_t             addrLen = strnlen(addr, 107);

        struct sockaddr_un unixAddr;
        unixAddr.sun_family = AF_UNIX;
        strncpy(unixAddr.sun_path, addr, addrLen);
        if (unixAddr.sun_path[0] == '@')
            unixAddr.sun_path[0] = '\0';

        if (connect(fd, (const sockaddr*)&unixAddr, sizeof(struct sockaddr_un)) < 0)
            return -errno;

        // arbitrary value which seems to be enough for s-d messages
        ssize_t stateLen = strnlen(state, 128);
        if (write(fd, state, stateLen) == stateLen)
            return 1;

        return -errno;
    }
}
