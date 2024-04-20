#include "SdDaemon.hpp"

#include <memory>

#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

namespace Systemd {
    int SdBooted(void) {
        if (!faccessat(AT_FDCWD, "/run/systemd/system/", F_OK, AT_SYMLINK_NOFOLLOW))
            return true;

        if (errno == ENOENT)
            return false;

        return -errno;
    }

    int SdNotify(int unset_environment, const char* state) {
        int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
        if (fd == -1)
            return -errno;

        constexpr char env_var[] = "NOTIFY_SOCKET";

        auto           cleanup = [unset_environment, env_var](int* fd) {
            if (unset_environment)
                unsetenv(env_var);
            close(*fd);
        };
        std::unique_ptr<int, decltype(cleanup)> fd_cleanup(&fd, cleanup);

        const char*                             addr = getenv(env_var);
        if (!addr)
            return 0;

        // address length must be at most this; see man 7 unix
        size_t             addr_len = strnlen(addr, 107);

        struct sockaddr_un unix_addr;
        unix_addr.sun_family = AF_UNIX;
        strncpy(unix_addr.sun_path, addr, addr_len);
        if (unix_addr.sun_path[0] == '@')
            unix_addr.sun_path[0] = '\0';

        if (!connect(fd, (const sockaddr*)&unix_addr, sizeof(struct sockaddr_un)))
            return 1;

        // arbitrary value which seems to be enough for s-d messages
        size_t state_len = strnlen(state, 128);
        if (write(fd, state, state_len) >= 0)
            return 1;

        return -errno;
    }
}
