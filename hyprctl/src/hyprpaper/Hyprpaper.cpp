#include "Hyprpaper.hpp"
#include "../helpers/Memory.hpp"

#include <optional>
#include <format>
#include <filesystem>

#include <hyprpaper_core-client.hpp>
#include <hp_hyprtavern_core_v1-client.hpp>

#include <hyprutils/string/VarList2.hpp>
using namespace Hyprutils::String;

using namespace std::string_literals;

constexpr const char*               SOCKET_NAME = ".hyprpaper.sock";
static SP<CCHyprpaperCoreImpl>      g_coreImpl;
static SP<CCHpHyprtavernCoreV1Impl> g_tavernImpl;

constexpr const uint32_t            HYPRPAPER_PROTOCOL_VERSION_SUPPORTED  = 1;
constexpr const uint32_t            HYPRTAVERN_PROTOCOL_VERSION_SUPPORTED = 1;

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

static std::expected<std::string, std::string> resolvePath(const std::string_view& sv) {
    std::error_code ec;
    auto            can = std::filesystem::canonical(sv, ec);

    if (ec)
        return std::unexpected(std::format("invalid path: {}", ec.message()));

    return can;
}

static std::expected<std::string, std::string> getFullPath(const std::string_view& sv) {
    if (sv.empty())
        return std::unexpected("empty path");

    if (sv[0] == '~') {
        static auto HOME = getenv("HOME");
        if (!HOME || HOME[0] == '\0')
            return std::unexpected("home path but no $HOME");

        return resolvePath(std::string{HOME} + "/"s + std::string{sv.substr(1)});
    }

    return resolvePath(sv);
}

static std::expected<void, std::string> makeRequestToSocket(SP<Hyprwire::IClientSocket> socket, const std::string& PATH, const std::string& MONITOR, const std::string_view& FIT) {
    g_coreImpl = makeShared<CCHyprpaperCoreImpl>(HYPRPAPER_PROTOCOL_VERSION_SUPPORTED);

    socket->addImplementation(g_coreImpl);

    if (!socket->waitForHandshake())
        return std::unexpected("can't send: wire handshake failed");

    auto spec = socket->getSpec(g_coreImpl->protocol()->specName());

    if (!spec)
        return std::unexpected("can't send: hyprpaper doesn't have the spec?!");

    auto manager = makeShared<CCHyprpaperCoreManagerObject>(socket->bindProtocol(g_coreImpl->protocol(), HYPRPAPER_PROTOCOL_VERSION_SUPPORTED));

    if (!manager)
        return std::unexpected("wire error: couldn't create manager");

    auto wallpaper = makeShared<CCHyprpaperWallpaperObject>(manager->sendGetWallpaperObject());

    if (!wallpaper)
        return std::unexpected("wire error: couldn't create wallpaper object");

    std::optional<std::string> err;

    wallpaper->setFailed([&err](uint32_t code) { err = std::format("failed to set wallpaper, code {}", code); });

    wallpaper->sendPath(PATH.c_str());
    wallpaper->sendMonitorName(MONITOR.c_str());
    if (!FIT.empty())
        wallpaper->sendFitMode(fitFromString(FIT));

    wallpaper->sendApply();

    socket->roundtrip();

    if (err)
        return std::unexpected(*err);

    return {};
}

static bool attemptHyprtavern(const std::string& PATH, const std::string& MONITOR, const std::string_view& FIT) {
    // try to connect to hyprtavern
    g_tavernImpl = makeShared<CCHpHyprtavernCoreV1Impl>(HYPRTAVERN_PROTOCOL_VERSION_SUPPORTED);

    const auto RTDIR = getenv("XDG_RUNTIME_DIR");

    auto       socketPath = RTDIR + "/hyprtavern/ht.sock"s;

    auto       socket = Hyprwire::IClientSocket::open(socketPath);

    if (!socket)
        return false;

    socket->addImplementation(g_tavernImpl);

    if (!socket->waitForHandshake())
        return false;

    auto spec = socket->getSpec(g_tavernImpl->protocol()->specName());

    if (!spec)
        return false;

    auto manager = makeShared<CCHpHyprtavernCoreManagerV1Object>(socket->bindProtocol(g_tavernImpl->protocol(), HYPRTAVERN_PROTOCOL_VERSION_SUPPORTED));

    if (!manager)
        return false;

    // run a query, find all possible candidates
    const auto WAYLAND_DISPLAY = getenv("WAYLAND_DISPLAY");

    if (!WAYLAND_DISPLAY)
        return false;

    std::string              propStr = std::format("GLOBAL:WAYLAND_DISPLAY={}", WAYLAND_DISPLAY);

    std::vector<const char*> props  = {propStr.c_str()};
    std::vector<const char*> protos = {"hyprpaper_core"};

    auto                     query = makeShared<CCHpHyprtavernBusQueryV1Object>(
        manager->sendGetQueryObject(protos, HP_HYPRTAVERN_CORE_V1_BUS_QUERY_FILTER_MODE_ALL, props, HP_HYPRTAVERN_CORE_V1_BUS_QUERY_FILTER_MODE_ALL));

    uint32_t candidateId = 0;

    query->setResults([&candidateId](const std::vector<uint32_t>& vec) {
        if (vec.empty())
            return;

        candidateId = vec.at(0);
    });

    socket->roundtrip();

    if (candidateId == 0)
        return false; // no hyprpaper-compatible thing in the tavern

    auto handle = makeShared<CCHpHyprtavernBusObjectHandleV1Object>(manager->sendGetObjectHandle(candidateId));

    if (!handle)
        return false;

    int newSocketFd = -1;

    handle->setSocket([&newSocketFd](int fd) { newSocketFd = fd; });

    handle->sendConnect();

    socket->roundtrip();

    if (newSocketFd < 0)
        return false;

    auto newSocket = Hyprwire::IClientSocket::open(newSocketFd);

    if (!newSocket)
        return false;

    return !!makeRequestToSocket(newSocket, PATH, MONITOR, FIT);
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

    const std::string MONITOR  = std::string{args[0]};
    const auto&       PATH_RAW = args[1];
    const auto&       FIT      = args[2];

    if (PATH_RAW.empty())
        return std::unexpected("not enough args");

    const auto RTDIR = getenv("XDG_RUNTIME_DIR");

    if (!RTDIR || RTDIR[0] == '\0')
        return std::unexpected("can't send: no XDG_RUNTIME_DIR");

    const auto HIS = getenv("HYPRLAND_INSTANCE_SIGNATURE");

    if (!HIS || HIS[0] == '\0')
        return std::unexpected("can't send: no HYPRLAND_INSTANCE_SIGNATURE (not running under hyprland)");

    const auto PATH = getFullPath(PATH_RAW);

    if (!PATH)
        return std::unexpected(std::format("bad path: {}", PATH_RAW));

    if (attemptHyprtavern(*PATH, MONITOR, FIT))
        return {};

    auto socketPath = RTDIR + "/hypr/"s + HIS + "/"s + SOCKET_NAME;

    auto socket = Hyprwire::IClientSocket::open(socketPath);

    if (!socket)
        return std::unexpected("can't send: failed to connect to hyprpaper (is it running?)");

    return makeRequestToSocket(socket, *PATH, MONITOR, FIT);
}