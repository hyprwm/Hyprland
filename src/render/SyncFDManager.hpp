#pragma once

#include <hyprutils/os/FileDescriptor.hpp>

namespace Render {
    class ISyncFDManager {
      public:
        virtual ~ISyncFDManager() = default;

        Hyprutils::OS::CFileDescriptor&  fd();
        Hyprutils::OS::CFileDescriptor&& takeFd();
        virtual bool                     isValid();

      protected:
        ISyncFDManager() = default;

        Hyprutils::OS::CFileDescriptor m_fd;
        bool                           m_valid = false;
    };
}
