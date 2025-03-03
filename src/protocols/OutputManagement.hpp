#pragma once

#include <vector>
#include <cstdint>
#include <unordered_map>
#include "WaylandProtocol.hpp"
#include "wlr-output-management-unstable-v1.hpp"
#include "../helpers/signal/Signal.hpp"
#include <aquamarine/output/Output.hpp>

class CMonitor;

class COutputHead;
class COutputMode;

struct SMonitorRule;

enum eWlrOutputCommittedProperties : uint32_t {
    OUTPUT_HEAD_COMMITTED_MODE          = (1 << 0),
    OUTPUT_HEAD_COMMITTED_CUSTOM_MODE   = (1 << 1),
    OUTPUT_HEAD_COMMITTED_POSITION      = (1 << 2),
    OUTPUT_HEAD_COMMITTED_TRANSFORM     = (1 << 3),
    OUTPUT_HEAD_COMMITTED_SCALE         = (1 << 4),
    OUTPUT_HEAD_COMMITTED_ADAPTIVE_SYNC = (1 << 5),
};

struct SWlrManagerOutputState {
    uint32_t        committedProperties = 0;

    WP<COutputMode> mode;
    struct {
        Vector2D size;
        uint32_t refresh = 0;
    } customMode;
    Vector2D            position;
    wl_output_transform transform    = WL_OUTPUT_TRANSFORM_NORMAL;
    float               scale        = 1.F;
    bool                adaptiveSync = false;
    bool                enabled      = true;
};

struct SWlrManagerSavedOutputState {
    uint32_t            committedProperties = 0;
    Vector2D            resolution;
    uint32_t            refresh = 0;
    Vector2D            position;
    wl_output_transform transform    = WL_OUTPUT_TRANSFORM_NORMAL;
    float               scale        = 1.F;
    bool                adaptiveSync = false;
    bool                enabled      = true;
};

class COutputManager {
  public:
    COutputManager(SP<CZwlrOutputManagerV1> resource_);

    bool good();
    void ensureMonitorSent(PHLMONITOR pMonitor);
    void sendDone();

    // holds the states for this manager.
    std::unordered_map<std::string, SWlrManagerSavedOutputState> monitorStates;

  private:
    SP<CZwlrOutputManagerV1>     resource;
    bool                         stopped = false;

    WP<COutputManager>           self;

    std::vector<WP<COutputHead>> heads;

    void                         makeAndSendNewHead(PHLMONITOR pMonitor);
    friend class COutputManagementProtocol;
};

class COutputMode {
  public:
    COutputMode(SP<CZwlrOutputModeV1> resource_, SP<Aquamarine::SOutputMode> mode_);

    bool                        good();
    SP<Aquamarine::SOutputMode> getMode();
    void                        sendAllData();

  private:
    SP<CZwlrOutputModeV1>       resource;
    WP<Aquamarine::SOutputMode> mode;

    friend class COutputHead;
    friend class COutputManagementProtocol;
};

class COutputHead {
  public:
    COutputHead(SP<CZwlrOutputHeadV1> resource_, PHLMONITOR pMonitor_);

    bool       good();
    void       sendAllData(); // this has to be separate as we need to send the head first, then set the data
    void       updateMode();
    PHLMONITOR monitor();

  private:
    SP<CZwlrOutputHeadV1>        resource;
    PHLMONITORREF                pMonitor;

    void                         makeAndSendNewMode(SP<Aquamarine::SOutputMode> mode);
    void                         sendCurrentMode();

    std::vector<WP<COutputMode>> modes;

    struct {
        CHyprSignalListener monitorDestroy;
        CHyprSignalListener monitorModeChange;
        m_m_listeners;

        friend class COutputManager;
        friend class COutputManagementProtocol;
    };

    class COutputConfigurationHead {
      public:
        COutputConfigurationHead(SP<CZwlrOutputConfigurationHeadV1> resource_, PHLMONITOR pMonitor_);

        bool                   good();

        SWlrManagerOutputState state;

      private:
        SP<CZwlrOutputConfigurationHeadV1> resource;
        PHLMONITORREF                      pMonitor;

        friend class COutputConfiguration;
    };

    class COutputConfiguration {
      public:
        COutputConfiguration(SP<CZwlrOutputConfigurationV1> resource_, SP<COutputManager> owner_);

        bool good();

      private:
        SP<CZwlrOutputConfigurationV1>            resource;
        std::vector<WP<COutputConfigurationHead>> heads;
        WP<COutputManager>                        owner;

        bool                                      applyTestConfiguration(bool test);
    };

    class COutputManagementProtocol : public IWaylandProtocol {
      public:
        COutputManagementProtocol(const wl_interface* iface, const int& ver, const std::string& name);

        virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

        // doesn't have to return one
        SP<SWlrManagerSavedOutputState> getOutputStateFor(PHLMONITOR pMonitor);

      private:
        void destroyResource(COutputManager* resource);
        void destroyResource(COutputHead* resource);
        void destroyResource(COutputMode* resource);
        void destroyResource(COutputConfiguration* resource);
        void destroyResource(COutputConfigurationHead* resource);

        void updateAllOutputs();

        //
        std::vector<SP<COutputManager>>           m_vManagers;
        std::vector<SP<COutputHead>>              m_vHeads;
        std::vector<SP<COutputMode>>              m_vModes;
        std::vector<SP<COutputConfiguration>>     m_vConfigurations;
        std::vector<SP<COutputConfigurationHead>> m_vConfigurationHeads;

        SP<COutputHead>                           headFromResource(wl_resource* r);
        SP<COutputMode>                           modeFromResource(wl_resource* r);

        friend class COutputManager;
        friend class COutputHead;
        friend class COutputMode;
        friend class COutputConfiguration;
        friend class COutputConfigurationHead;
    };

    namespace PROTO {
        inline UP<COutputManagementProtocol> outputManagement;
    };
