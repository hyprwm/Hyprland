#include "DonationNagManager.hpp"
#include "../debug/Log.hpp"
#include "VersionKeeperManager.hpp"
#include "eventLoop/EventLoopManager.hpp"

#include <chrono>
#include <format>

#include "../helpers/fs/FsUtils.hpp"

#include <hyprutils/os/Process.hpp>
using namespace Hyprutils::OS;

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
    if (g_pVersionKeeperMgr->fired())
        return;

    const auto DATAROOT = NFsUtils::getDataHome();

    if (!DATAROOT)
        return;

    const auto EPOCH = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    const auto LASTNAGSTR = NFsUtils::readFileAsString(*DATAROOT + "/" + LAST_NAG_FILE_NAME);

    if (!LASTNAGSTR) {
        const auto EPOCHSTR = std::format("{}", EPOCH);
        NFsUtils::writeToFile(*DATAROOT + "/" + LAST_NAG_FILE_NAME, EPOCHSTR);
        return;
    }

    uint64_t LAST_EPOCH = 0;

    try {
        LAST_EPOCH = std::stoull(*LASTNAGSTR);
    } catch (std::exception& e) {
        Debug::log(ERR, "DonationNag: Last epoch invalid? Failed to parse \"{}\". Setting to today.", *LASTNAGSTR);
        const auto EPOCHSTR = std::format("{}", EPOCH);
        NFsUtils::writeToFile(*DATAROOT + "/" + LAST_NAG_FILE_NAME, EPOCHSTR);
        return;
    }

    // don't nag if the last nag was less than a month ago. This is
    // mostly for first-time nags, as other nags happen in specific time frames shorter than a month
    if (EPOCH - LAST_EPOCH < MONTH_IN_SECONDS) {
        Debug::log(LOG, "DonationNag: last nag was {} days ago, too early for a nag.", (int)std::round((EPOCH - LAST_EPOCH) / (double)MONTH_IN_SECONDS));
        return;
    }

    if (!NFsUtils::executableExistsInPath("hyprland-donation-screen")) {
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

        m_bFired = true;

        const auto EPOCHSTR = std::format("{}", EPOCH);
        NFsUtils::writeToFile(*DATAROOT + "/" + LAST_NAG_FILE_NAME, EPOCHSTR);

        g_pEventLoopManager->doLater([] {
            CProcess proc("hyprland-donation-screen", {});
            proc.runAsync();
        });

        break;
    }

    if (!m_bFired)
        Debug::log(LOG, "DonationNag: didn't hit any nagging periods");
}

bool CDonationNagManager::fired() {
    return m_bFired;
}