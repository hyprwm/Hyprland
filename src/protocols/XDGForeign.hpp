#include <hyprutils/signal/Listener.hpp>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <optional>
#include <map>
#include "WaylandProtocol.hpp"
#include "xdg-foreign-unstable-v2.hpp"
#include "../helpers/signal/Signal.hpp"

class CXDGForeignResource;
class CXDGImportedResource;
class CWLSurface;
class CXDGToplevelResource;

// zxdg_exported_v*
class CXDGExportedResource {
  public:
    CXDGExportedResource(SP<CZxdgExportedV2> resource_, SP<CXDGToplevelResource> surface_, const std::string& handle);
    //~CXDGExportedResource();

    bool                     good();
    std::string_view         handle();
    WP<CXDGToplevelResource> xdgSurf();

    struct {
        CSignalT<> destroy;
    } m_events;

  private:
    CHyprSignalListener      m_xdgDestroyListener;
    SP<CZxdgExportedV2>      m_resource;
    WP<CXDGToplevelResource> m_toplevel;
    std::string              m_handle;

    friend class CXDGForeignExporterProtocol;
};

// zxdg_imported_v*
class CXDGImportedResource {
  public:
    CXDGImportedResource(SP<CZxdgImportedV2> resource_, const std::string& handle);
    // ~CXDGImportedResource();

    bool                     good();
    WP<CXDGExportedResource> exported;

  private:
    SP<CZxdgImportedV2> m_resource;
    std::string         m_handle;
    CHyprSignalListener m_exportDestoryListener;

    void                onSetParentOf(SP<CWLSurface> child);
    friend class CXDGForeignImporterProtocol;
};

// zxdg_exporter_v
class CXDGForeignExporterProtocol : public IWaylandProtocol {
  public:
    CXDGForeignExporterProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void                                              bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    std::unordered_map<std::string, SP<CZxdgExportedV2>>      m;
    std::unordered_map<std::string, UP<CXDGExportedResource>> mv2;

  private:
    void                             destroyExported(CXDGExportedResource*);
    void                             onExporterDestroyed(CZxdgExporterV2*);
    std::vector<UP<CZxdgExporterV2>> m_exporters;

    friend class CXDGExportedResource;
    friend class CXDGForeignImporterProtocol;
};

// zxdg_importer_v*
class CXDGForeignImporterProtocol : public IWaylandProtocol {
  public:
    CXDGForeignImporterProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void                                                      destoryImported(CXDGImportedResource*);
    void                                                      onImporterDestroyed(CZxdgImporterV2*);
    std::vector<UP<CZxdgImporterV2>>                          m_importers;
    std::unordered_map<std::string, SP<CXDGImportedResource>> m_imports;

    SP<CXDGExportedResource>                                  findExport(const std::string& handle);
    std::string                                               generateHandle();

    friend class CXDGImportedResource;
    friend class CXDGForeignExporterProtocol;
};

namespace PROTO {
    inline UP<CXDGForeignExporterProtocol> xdgForeignExporter;
    inline UP<CXDGForeignImporterProtocol> xdgForeignImporter;
};