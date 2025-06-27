#pragma once

#include "WaylandProtocol.hpp"
#include "../desktop/DesktopTypes.hpp"
#include "ext-workspace-v1.hpp"
#include <cstdint>
#include <vector>
#include "../helpers/signal/Signal.hpp"
#include "../helpers/Monitor.hpp"

class CExtWorkspaceManagerResource;

class CExtWorkspaceGroupResource {
  public:
    CExtWorkspaceGroupResource(WP<CExtWorkspaceManagerResource> manager, UP<CExtWorkspaceGroupHandleV1> resource, PHLMONITORREF monitor);

    static WP<CExtWorkspaceGroupResource> fromResource(wl_resource*);

    [[nodiscard]] bool                    good() const;

    void                                  workspaceEnter(const WP<CExtWorkspaceHandleV1>&);
    void                                  workspaceLeave(const WP<CExtWorkspaceHandleV1>&);

    PHLMONITORREF                         m_monitor;

  private:
    WP<CExtWorkspaceGroupResource>   m_self;
    WP<CExtWorkspaceManagerResource> m_manager;
    UP<CExtWorkspaceGroupHandleV1>   m_resource;

    struct {
        CHyprSignalListener destroyed;
        CHyprSignalListener outputBound;
    } m_listeners;

    friend class CExtWorkspaceManagerResource;
};

class CExtWorkspaceResource {
  public:
    CExtWorkspaceResource(WP<CExtWorkspaceManagerResource> manager, UP<CExtWorkspaceHandleV1> resource, PHLWORKSPACEREF workspace);

    [[nodiscard]] bool good() const;

    void               commit();

  private:
    WP<CExtWorkspaceResource>        m_self;
    WP<CExtWorkspaceManagerResource> m_manager;
    UP<CExtWorkspaceHandleV1>        m_resource;
    WP<CExtWorkspaceGroupResource>   m_group;
    PHLWORKSPACEREF                  m_workspace;

    [[nodiscard]] bool               isActive() const;

    void                             sendState();
    void                             sendCapabilities();
    void                             sendGroup();

    struct {
        bool          activate   = false;
        bool          deactivate = false;
        PHLMONITORREF targetMonitor;
    } m_pendingState;

    struct {
        CHyprSignalListener destroyed;
        CHyprSignalListener activeChanged;
        CHyprSignalListener monitorChanged;
        CHyprSignalListener renamed;
    } m_listeners;

    friend class CExtWorkspaceManagerResource;
};

class CExtWorkspaceManagerResource {
  public:
    CExtWorkspaceManagerResource(UP<CExtWorkspaceManagerV1> resource);
    WP<CExtWorkspaceManagerResource>             m_self;

    void                                         init(WP<CExtWorkspaceManagerResource> self);
    [[nodiscard]] bool                           good() const;

    void                                         onMonitorCreated(const PHLMONITOR& monitor);
    void                                         onWorkspaceCreated(const PHLWORKSPACE& workspace);

    void                                         scheduleDone();
    [[nodiscard]] WP<CExtWorkspaceGroupResource> findGroup(const PHLMONITORREF& monitor) const;
    void                                         sendGroupToWorkspaces(const WP<CExtWorkspaceGroupResource>& group);

    UP<CExtWorkspaceManagerV1>                   m_resource;

  private:
    bool m_doneScheduled = false;
};

class CExtWorkspaceProtocol : public IWaylandProtocol {
  public:
    CExtWorkspaceProtocol(const wl_interface* iface, const int& var, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    void         destroyManager(const WP<CExtWorkspaceManagerResource>& manager);
    void         destroyGroup(const WP<CExtWorkspaceGroupResource>& group);
    void         destroyWorkspace(const WP<CExtWorkspaceResource>& workspace);

  private:
    std::vector<UP<CExtWorkspaceManagerResource>> m_managers;
    std::vector<UP<CExtWorkspaceGroupResource>>   m_groups;
    std::vector<UP<CExtWorkspaceResource>>        m_workspaces;

    friend class CExtWorkspaceManagerResource;
};

namespace PROTO {
    inline UP<CExtWorkspaceProtocol> extWorkspace;
}
