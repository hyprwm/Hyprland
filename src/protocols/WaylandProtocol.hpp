#pragma once

#include "../defines.hpp"

#include <functional>

#define RESOURCE_OR_BAIL(resname)                                                                                                                                                  \
    const auto resname = (CWaylandResource*)wl_resource_get_user_data(resource);                                                                                                   \
    if (!resname)                                                                                                                                                                  \
        return;

#define PROTO NProtocols

class IWaylandProtocol {
  public:
    IWaylandProtocol(const wl_interface* iface, const int& ver, const std::string& name);
    virtual ~IWaylandProtocol();

    virtual void onDisplayDestroy();

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) = 0;

    template <typename... Args>
    void protoLog(LogLevel level, std::format_string<Args...> fmt, Args&&... args) {
        Debug::log(level, std::format("[{}] ", m_szName) + std::vformat(fmt.get(), std::make_format_args(args...)));
    };

  private:
    std::string m_szName;
    wl_global*  m_pGlobal = nullptr;
    wl_listener m_liDisplayDestroy;
};