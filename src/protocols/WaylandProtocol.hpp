#pragma once

#include "../defines.hpp"

#include <functional>

#define RESOURCE_OR_BAIL(resname)                                                                                                                                                  \
    const auto resname = (CWaylandResource*)wl_resource_get_user_data(resource);                                                                                                   \
    if (!resname)                                                                                                                                                                  \
        return;

#define PROTO NProtocols

#define EXTRACT_CLASS_NAME()                                                                                                                                                       \
    []() constexpr -> std::string_view {                                                                                                                                           \
        constexpr std::string_view prettyFunction = __PRETTY_FUNCTION__;                                                                                                           \
        constexpr size_t           colons         = prettyFunction.find("::");                                                                                                     \
        if (colons != std::string_view::npos) {                                                                                                                                    \
            constexpr size_t begin = prettyFunction.substr(0, colons).rfind(' ') + 1;                                                                                              \
            constexpr size_t end   = colons - begin;                                                                                                                               \
            return prettyFunction.substr(begin, end);                                                                                                                              \
        } else {                                                                                                                                                                   \
            return "Global";                                                                                                                                                       \
        }                                                                                                                                                                          \
    }()

#define LOGM(level, ...)                                                                                                                                                           \
    do {                                                                                                                                                                           \
        std::ostringstream oss;                                                                                                                                                    \
        if (level == WARN || level == ERR || level == CRIT) {                                                                                                                      \
            oss << "[" << __FILE__ << ":" << __LINE__ << "] ";                                                                                                                     \
        } else if (level == LOG || level == INFO || level == TRACE) {                                                                                                              \
            oss << "[" << EXTRACT_CLASS_NAME() << "] ";                                                                                                                            \
        }                                                                                                                                                                          \
        if constexpr (std::is_same_v<decltype(__VA_ARGS__), std::string>) {                                                                                                        \
            oss << __VA_ARGS__;                                                                                                                                                    \
            Debug::log(level, oss.str());                                                                                                                                          \
        } else {                                                                                                                                                                   \
            Debug::log(level, std::format("{}{}", oss.str(), std::format(__VA_ARGS__)));                                                                                           \
        }                                                                                                                                                                          \
    } while (0)

class IWaylandProtocol;
struct IWaylandProtocolDestroyWrapper {
    wl_listener       listener;
    IWaylandProtocol* parent = nullptr;
};

class IWaylandProtocol {
  public:
    IWaylandProtocol(const wl_interface* iface, const int& ver, const std::string& name);
    virtual ~IWaylandProtocol();

    virtual void                   onDisplayDestroy();
    virtual void                   removeGlobal();

    virtual void                   bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) = 0;

    IWaylandProtocolDestroyWrapper m_liDisplayDestroy;

  private:
    std::string m_szName;
    wl_global*  m_pGlobal = nullptr;
};
