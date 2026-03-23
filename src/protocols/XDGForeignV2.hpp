#include <hyprutils/signal/Listener.hpp>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "WaylandProtocol.hpp"
#include "xdg-foreign-unstable-v2.hpp"
#include "../helpers/signal/Signal.hpp"

class CXDGImportedResourceV2;
class CWLSurface;
class CXDGToplevelResource;

class CXDGExportedResourceV2 {
  public:
    CXDGExportedResourceV2(SP<CZxdgExportedV2> resource_, SP<CXDGToplevelResource> surface_, const std::string& handle);
    ~CXDGExportedResourceV2();

    struct {
        CSignalT<> destroy;
    } m_events;

    bool                     good() const;
    std::string_view         handle() const;
    WP<CXDGToplevelResource> xdgSurf() const;

  private:
    bool m_topLevelDestroyed = false;
    struct {
        CHyprSignalListener topLevelDestroyed;
    } m_listeners;
    SP<CZxdgExportedV2>      m_resource;
    WP<CXDGToplevelResource> m_toplevel;
    std::string              m_handle;

    friend class CXDGForeignExporterProtocolV2;
};

// zxdg_imported_v2
class CXDGImportedResourceV2 {
  public:
    CXDGImportedResourceV2(SP<CZxdgImportedV2> resource, SP<CXDGExportedResourceV2> exported, const std::string& handle);
    ~CXDGImportedResourceV2();

  private:
    SP<CZxdgImportedV2>        m_resource;
    WP<CXDGExportedResourceV2> m_exported;
    std::string                m_handle;

    struct {
        CHyprSignalListener exportedDestroyed;
    } m_listeners;

    friend class CXDGForeignImporterProtocolV2;
};

class CXDGForeignExporterProtocolV2 : public IWaylandProtocol {
  public:
    CXDGForeignExporterProtocolV2(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void               bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) override;

    SP<CXDGExportedResourceV2> getExported(const std::string& handle) const;

  private:
    void                                                        destroyExported(CXDGExportedResourceV2*);
    void                                                        onExporterDestroyed(CZxdgExporterV2*);
    std::vector<UP<CZxdgExporterV2>>                            m_exporters;
    std::unordered_map<std::string, SP<CXDGExportedResourceV2>> m_exported;

    friend class CXDGExportedResourceV2;
};

class CXDGForeignImporterProtocolV2 : public IWaylandProtocol {
  public:
    CXDGForeignImporterProtocolV2(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) override;

  private:
    void                                    destroyImported(CXDGImportedResourceV2*);
    void                                    onImporterDestroyed(CZxdgImporterV2*);
    std::vector<UP<CZxdgImporterV2>>        m_importers;
    std::vector<UP<CXDGImportedResourceV2>> m_imports;

    friend class CXDGImportedResourceV2;
};

namespace PROTO {
    inline UP<CXDGForeignExporterProtocolV2> xdgForeignExporter;
    inline UP<CXDGForeignImporterProtocolV2> xdgForeignImporter;
};