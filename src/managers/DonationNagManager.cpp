#include "DonationNagManager.hpp"
#include "../debug/Log.hpp"
#include "VersionKeeperManager.hpp"
#include "eventLoop/EventLoopManager.hpp"
#include "../config/ConfigValue.hpp"

#include <chrono>
#include <format>

#include "../helpers/fs/FsUtils.hpp"

#include <hyprutils/os/Process.hpp>
#include <hyprutils/string/VarList.hpp>
using namespace Hyprutils::OS;
using namespace Hyprutils::String;

constexpr const char* LAST_NAG_FILE_NAME = "lastNag";
constexpr uint64_t    DAY_IN_SECONDS     = 3600ULL * 24;
constexpr uint64_t    MONTH_IN_SECONDS   = DAY_IN_SECONDS * 30;

struct SNagDatePoint {
    // Counted from 1, as in Jan 1st is 1, 1
    // No month-boundaries because I am lazy
    uint8_t month = 0, dayStart = 0, dayEnd = 0;
};

// clang-format off
const std::vector<SNagDatePoint> NAG_DATE_POINTS = {
    SNagDatePoint {
        7, 20, 31,
    },
    SNagDatePoint {
        12, 1, 28
    },
};
// clang-format on

CDonationNagManager::CDonationNagManager() {
    static auto PNONAG = CConfigValue<Hyprlang::INT>("ecosystem:no_donation_nag");

    if (g_pVersionKeeperMgr->fired() || *PNONAG)
        return;

    const auto DATAROOT = NFsUtils::getDataHome();

    if (!DATAROOT)
        return;

    const auto EPOCH = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    uint64_t   currentMajor = 0;
    try {
        CVarList vl(HYPRLAND_VERSION, 0, '.');
        currentMajor = std::stoull(vl[1]);
    } catch (...) {
        // ????
        return;
    }

    auto state = getState();

    if ((!state.major && currentMajor <= 48) || !state.epoch) {
        state.major = currentMajor;
        state.epoch = state.epoch == 0 ? EPOCH : state.epoch;
        writeState(state);
        return;
    }

    // don't nag if the last nag was less than a month ago. This is
    // mostly for first-time nags, as other nags happen in specific time frames shorter than a month
    if (EPOCH - state.epoch < MONTH_IN_SECONDS) {
        Debug::log(LOG, "DonationNag: last nag was {} days ago, too early for a nag.", static_cast<int>(std::round((EPOCH - state.epoch) / static_cast<double>(DAY_IN_SECONDS))));
        return;
    }

    if (!NFsUtils::executableExistsInPath("hyprland-donate-screen")) {
        Debug::log(ERR, "DonationNag: executable doesn't exist, skipping.");
        return;
    }

    auto       tt    = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    auto       local = *localtime(&tt);

    const auto MONTH = local.tm_mon + 1;
    const auto DAY   = local.tm_mday;

    for (const auto& nagPoint : NAG_DATE_POINTS) {
        if (MONTH != nagPoint.month)
            continue;

        if (DAY < nagPoint.dayStart || DAY > nagPoint.dayEnd)
            continue;

        Debug::log(LOG, "DonationNag: hit nag month {} days {}-{}, it's {} today, nagging", MONTH, nagPoint.dayStart, nagPoint.dayEnd, DAY);

        fire();

        state.major = currentMajor;
        state.epoch = EPOCH;
        writeState(state);

        break;
    }

    if (!m_fired)
        Debug::log(LOG, "DonationNag: didn't hit any nagging periods, checking update");

    if (state.major < currentMajor) {
        Debug::log(LOG, "DonationNag: hit nag for major update {} -> {}", state.major, currentMajor);

        fire();

        state.major = currentMajor;
        state.epoch = EPOCH;
        writeState(state);
    }

    if (!m_fired)
        Debug::log(LOG, "DonationNag: didn't hit nagging conditions");
}

bool CDonationNagManager::fired() {
    return m_fired;
}

void CDonationNagManager::fire() {
    static const auto DATAROOT = NFsUtils::getDataHome();

    m_fired = true;

    g_pEventLoopManager->doLater([] {
        CProcess proc("hyprland-donate-screen", {});
        proc.runAsync();
    });
}

CDonationNagManager::SStateData CDonationNagManager::getState() {
    static const auto DATAROOT = NFsUtils::getDataHome();
    const auto        STR      = NFsUtils::readFileAsString(*DATAROOT + "/" + LAST_NAG_FILE_NAME);

    if (!STR.has_value())
        return {};

    CVarList                        lines(*STR, 0, '\n');
    CDonationNagManager::SStateData state;

    try {
        state.epoch = std::stoull(lines[0]);
        state.major = std::stoull(lines[1]);
    } catch (...) { ; }

    return state;
}

void CDonationNagManager::writeState(const SStateData& s) {
    static const auto DATAROOT = NFsUtils::getDataHome();
    NFsUtils::writeToFile(*DATAROOT + "/" + LAST_NAG_FILE_NAME, std::format("{}\n{}", s.epoch, s.major));
}
