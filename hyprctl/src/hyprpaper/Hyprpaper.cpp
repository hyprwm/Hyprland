#include "Hyprpaper.hpp"
#include "../helpers/Memory.hpp"

#include <optional>
#include <format>

#include <hyprpaper_core-client.hpp>

#include <hyprutils/string/VarList2.hpp>
using namespace Hyprutils::String;

using namespace std::string_literals;

constexpr const char*          SOCKET_NAME = ".hyprpaper.sock";
static SP<CCHyprpaperCoreImpl> g_coreImpl;

constexpr const uint32_t       PROTOCOL_VERSION_SUPPORTED = 1;

//
static hyprpaperCoreWallpaperFitMode fitFromString(const std::string_view& sv) {
    if (sv == "contain")
        return HYPRPAPER_CORE_WALLPAPER_FIT_MODE_CONTAIN;
    if (sv == "fit" || sv == "stretch")
        return HYPRPAPER_CORE_WALLPAPER_FIT_MODE_STRETCH;
    if (sv == "tile")
        return HYPRPAPER_CORE_WALLPAPER_FIT_MODE_TILE;
    return HYPRPAPER_CORE_WALLPAPER_FIT_MODE_COVER;
}

std::expected<void, std::string> Hyprpaper::makeHyprpaperRequest(const std::string_view& rq) {
    if (!rq.contains(' '))
        return std::unexpected("Invalid request");

    if (!rq.starts_with("/hyprpaper "))
        return std::unexpected("Invalid request");

    std::string_view LHS, RHS;
    auto             spacePos = rq.find(' ', 12);
    LHS                       = rq.substr(11, spacePos - 11);
    RHS                       = rq.substr(spacePos + 1);

    if (LHS != "wallpaper")
        return std::unexpected("Unknown hyprpaper request");

    CVarList2         args(std::string{RHS}, 0, ',');

    const std::string MONITOR = std::string{args[0]};
    const std::string PATH    = std::string{args[1]};
    const auto&       FIT     = args[2];

    const auto        RTDIR = getenv("XDG_RUNTIME_DIR");

    if (!RTDIR || RTDIR[0] == '\0')
        return std::unexpected("can't send: no XDG_RUNTIME_DIR");

    const auto HIS = getenv("HYPRLAND_INSTANCE_SIGNATURE");

    if (!HIS || HIS[0] == '\0')
        return std::unexpected("can't send: no HYPRLAND_INSTANCE_SIGNATURE (not running under hyprland)");

    auto socketPath = RTDIR + "/hypr/"s + HIS + "/"s + SOCKET_NAME;

    auto socket = Hyprwire::IClientSocket::open(socketPath);

    if (!socket)
        return std::unexpected("can't send: failed to connect to hyprpaper (is it running?)");

    g_coreImpl = makeShared<CCHyprpaperCoreImpl>(1);

    socket->addImplementation(g_coreImpl);

    if (!socket->waitForHandshake())
        return std::unexpected("can't send: wire handshake failed");

    auto spec = socket->getSpec(g_coreImpl->protocol()->specName());

    if (!spec)
        return std::unexpected("can't send: hyprpaper doesn't have the spec?!");

    auto manager = makeShared<CCHyprpaperCoreManagerObject>(socket->bindProtocol(g_coreImpl->protocol(), PROTOCOL_VERSION_SUPPORTED));

    if (!manager)
        return std::unexpected("wire error: couldn't create manager");

    auto wallpaper = makeShared<CCHyprpaperWallpaperObject>(manager->sendGetWallpaperObject());

    if (!wallpaper)
        return std::unexpected("wire error: couldn't create wallpaper object");

    bool                       canExit = false;
    std::optional<std::string> err;

    wallpaper->setFailed([&canExit, &err](uint32_t code) {
        canExit = true;
        err     = std::format("failed to set wallpaper, code {}", code);
    });
    wallpaper->setSuccess([&canExit]() { canExit = true; });

    wallpaper->sendPath(PATH.c_str());
    wallpaper->sendMonitorName(MONITOR.c_str());
    if (!FIT.empty())
        wallpaper->sendFitMode(fitFromString(FIT));

    wallpaper->sendApply();

    while (!canExit) {
        socket->dispatchEvents(true);
    }

    if (err)
        return std::unexpected(*err);

    return {};
}