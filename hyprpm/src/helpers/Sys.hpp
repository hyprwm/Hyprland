#pragma once

#include <string>

namespace NSys {
    bool isSuperuser();
    int  getUID();
    int  getEUID();

    // NOLINTNEXTLINE
    namespace root {
        void cacheSudo();
        void dropSudo();

        //
        [[nodiscard("Discarding could lead to vulnerabilities and bugs")]] bool createDirectory(const std::string& path, const std::string& mode);
        [[nodiscard("Discarding could lead to vulnerabilities and bugs")]] bool removeRecursive(const std::string& path);
        [[nodiscard("Discarding could lead to vulnerabilities and bugs")]] bool install(const std::string& what, const std::string& where, const std::string& mode);

        // Do not use this unless absolutely necessary!
        std::string runAsSuperuserUnsafe(const std::string& cmd);
    };
};