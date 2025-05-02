#include "CProgressBar.hpp"

#include <sys/ioctl.h>
#include <unistd.h>
#include <cmath>
#include <format>
#include <print>
#include <cstdio>

#include <algorithm>
#include <sstream>

#include "../helpers/Colors.hpp"

static winsize getTerminalSize() {
    winsize w{};
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w;
}

static void clearCurrentLine() {
    std::print("\r\33[2K"); // ansi escape sequence to clear entire line
}

void CProgressBar::printMessageAbove(const std::string& msg) {
    clearCurrentLine();
    std::print("\r{}\n", msg);

    print(); // reprint bar underneath
}

void CProgressBar::print() {
    const auto w = getTerminalSize();

    if (m_bFirstPrint) {
        std::print("\n");
        m_bFirstPrint = false;
    }

    clearCurrentLine();

    float percentDone = 0.0f;
    if (m_fPercentage >= 0.0f)
        percentDone = m_fPercentage;
    else {
        // check for divide-by-zero
        percentDone = m_iMaxSteps > 0 ? static_cast<float>(m_iSteps) / m_iMaxSteps : 0.0f;
    }
    // clamp to ensure no overflows (sanity check)
    percentDone = std::clamp(percentDone, 0.0f, 1.0f);

    const size_t       barWidth = std::clamp<size_t>(w.ws_col - m_szCurrentMessage.length() - 2, 0, 50);

    std::ostringstream oss;
    oss << ' ' << Colors::GREEN;

    size_t filled = static_cast<size_t>(std::floor(percentDone * barWidth));
    size_t i      = 0;

    for (; i < filled; ++i)
        oss << "━";

    if (i < barWidth) {
        oss << "╍" << Colors::RESET;
        ++i;
        for (; i < barWidth; ++i)
            oss << "━";
    } else
        oss << Colors::RESET;

    if (m_fPercentage >= 0.0f)
        oss << "  " << std::format("{}%", static_cast<int>(percentDone * 100.0)) << ' ';
    else
        oss << "  " << std::format("{} / {}", m_iSteps, m_iMaxSteps) << ' ';

    std::print("{} {}", oss.str(), m_szCurrentMessage);
    std::fflush(stdout);
}
