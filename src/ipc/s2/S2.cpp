#include "S2.hpp"

#include "Unix.hpp"

using namespace IPC;
using namespace IPC::Socket2;

UP<CSocket2>& IPC::Socket2::sock() {
    static auto p = makeUnique<CSocket2>();
    return p;
}

CSocket2::CSocket2() : m_impl(makeUnique<CUnixImpl>()) {
    ;
}

static std::string formatEvent(SEvent&& event) {
    std::string_view data        = event.data;
    const auto       LEN         = event.event.length();
    auto             eventString = std::format("{}>>{}\n", std::move(event.event), data.substr(0, 1024));
    std::replace(eventString.begin() + LEN + 2, eventString.end() - 1, '\n', ' ');
    return eventString;
}

void CSocket2::postEvent(SEvent&& event) {
    auto str = formatEvent(std::move(event));
    m_impl->send(std::move(str));
}
