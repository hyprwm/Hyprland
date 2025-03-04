#pragma once

#include <unordered_map>
#include "WaylandProtocol.hpp"
#include "idle-inhibit-unstable-v1.hpp"
#include "../helpers/signal/Signal.hpp"

class CIdleInhibitorResource;
class CWLSurfaceResource;

class CIdleInhibitor {
  public:
    CIdleInhibitor(SP<CIdleInhibitorResource> resource_, SP<CWLSurfaceResource> surf_);

    struct {
        CHyprSignalListener destroy;
        m_m_listeners;

        WP<CIdleInhibitorResource> resource;
        WP<CWLSurfaceResource>     surface;
    };

    class CIdleInhibitorResource {
      public:
        CIdleInhibitorResource(SP<CZwpIdleInhibitorV1> resource_, SP<CWLSurfaceResource> surface_);
        ~CIdleInhibitorResource();

        SP<CIdleInhibitor> inhibitor;

        struct {
            CSignal destroy;
        } events;

      private:
        SP<CZwpIdleInhibitorV1> resource;
        WP<CWLSurfaceResource>  surface;
        bool                    destroySent = false;

        struct {
            CHyprSignalListener destroySurface;
            m_m_listeners;
        };

        class CIdleInhibitProtocol : public IWaylandProtocol {
          public:
            CIdleInhibitProtocol(const wl_interface* iface, const int& ver, const std::string& name);

            virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

            struct {
                CSignal newIdleInhibitor; // data: SP<CIdleInhibitor>
            } events;

          private:
            void onManagerResourceDestroy(wl_resource* res);
            void onCreateInhibitor(CZwpIdleInhibitManagerV1* pMgr, uint32_t id, SP<CWLSurfaceResource> surface);

            void removeInhibitor(CIdleInhibitorResource*);

            //
            std::vector<UP<CZwpIdleInhibitManagerV1>> m_vManagers;
            std::vector<SP<CIdleInhibitorResource>>   m_vInhibitors;

            friend class CIdleInhibitorResource;
        };

        namespace PROTO {
            inline UP<CIdleInhibitProtocol> idleInhibit;
        }
