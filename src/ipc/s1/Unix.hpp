#pragma once

#include "Impl.hpp"

#include "../../helpers/memory/Memory.hpp"

#include <hyprutils/os/FileDescriptor.hpp>
#include <wayland-server-core.h>

#include <cstdint>
#include <string>
#include <vector>

class CEventLoopTimer;

namespace IPC::Socket1 {
    class CUnixImpl;

    class CUnixPeer {
      public:
        CUnixPeer(Hyprutils::OS::CFileDescriptor&& fd, pid_t pid, CUnixImpl& parent);
        ~CUnixPeer();

        void   init(const SP<CUnixPeer>& self);
        bool   good() const;
        size_t id() const;
        pid_t  pid() const;

        bool   readRequest(std::string& request);
        bool   setResponse(SResponse&& response);
        bool   addFollowData(std::string&& data);
        bool   flush();

        bool   shouldClose() const;
        bool   shouldFollow() const;
        bool   isFollowing() const;
        size_t queuedBytes() const;
        void   markFollowing();

      private:
        enum class eState : uint8_t {
            READING = 0,
            DEFERRED,
            WRITING,
            FOLLOWING,
        };

        void                           updateMask(uint32_t mask);

        Hyprutils::OS::CFileDescriptor m_fd;
        pid_t                          m_pid = 0;
        CUnixImpl&                     m_parent;
        wl_event_source*               m_eventSource = nullptr;
        SP<CEventLoopTimer>            m_requestTimer;
        WP<CUnixPeer>                  m_self;

        std::string                    m_input;
        std::string                    m_output;
        size_t                         m_writeOffset = 0;
        eState                         m_state       = eState::READING;
        eReplyMode                     m_replyMode   = eReplyMode::CLOSE;
    };

    class CUnixImpl : public IImplementation {
      public:
        CUnixImpl();
        virtual ~CUnixImpl();

        virtual void start(FRequestHandler&& handler) override;

        int          onServerEvent(uint32_t mask);
        int          onClientEvent(int fd, uint32_t mask);
        void         removeById(size_t id);
        void         completeDeferred(const WP<CUnixPeer>& peer, SP<CPromiseResult<std::string>> result, eReplyMode mode);

      private:
        SP<CUnixPeer>                  findByFd(int fd) const;
        void                           handleResponse(const SP<CUnixPeer>& peer);
        void                           beginFollowing(const SP<CUnixPeer>& peer);
        void                           flushFollowers();
        bool                           hasFollowers() const;

        Hyprutils::OS::CFileDescriptor m_socket;
        wl_event_source*               m_eventSource = nullptr;
        std::string                    m_socketPath;
        FRequestHandler                m_requestHandler;
        std::vector<SP<CUnixPeer>>     m_peers;
        SP<CEventLoopTimer>            m_followTimer;
    };
}
