#pragma once

#include "Impl.hpp"

#include "../../helpers/memory/Memory.hpp"

#include <hyprutils/os/FileDescriptor.hpp>
#include <wayland-server-core.h>

#include <vector>

namespace IPC::Socket2 {
    class CUnixPeer : public IClient {
      public:
        CUnixPeer(Hyprutils::OS::CFileDescriptor&& fd, void* parent);
        virtual ~CUnixPeer();

        virtual size_t id() const override;

        bool           flush();
        size_t         queueSize() const;
        bool           addEvent(const SP<std::string>&);

      private:
        Hyprutils::OS::CFileDescriptor m_fd;
        std::vector<SP<std::string>>   m_events;
        wl_event_source*               m_eventSource = nullptr;
        size_t                         m_writeOffset = 0;
    };

    class CUnixImpl : public IImplementation {
      public:
        CUnixImpl();
        virtual ~CUnixImpl();

        virtual bool send(std::string&& x) override;

        int          onServerEvent(int fd, uint32_t mask);
        int          onClientEvent(int fd, uint32_t mask);

      private:
        void                           removeByFd(int fd);
        SP<CUnixPeer>                  findByFd(int fd) const;

        Hyprutils::OS::CFileDescriptor m_socket;
        wl_event_source*               m_eventSource = nullptr;

        std::vector<SP<CUnixPeer>>     m_peers;
    };
};