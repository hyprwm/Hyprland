#include "CProgressBar.hpp"

#include <iostream>
#include <algorithm>
#include <cmath>
#include <format>

#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>

#include "../helpers/Colors.hpp"

void CProgressBar::printMessageAbove(const std::string& msg) {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    std::string spaces;
    for (size_t i = 0; i < w.ws_col; ++i) {
        spaces += ' ';
    }

    std::cout << "\r" << spaces << "\r" << msg << "\n";
    print();
}

void CProgressBar::print() {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    if (m_bFirstPrint)
        std::cout << "\n";
    m_bFirstPrint = false;

    std::string spaces;
    for (size_t i = 0; i < w.ws_col; ++i) {
        spaces += ' ';
    }

    std::cout << "\r" << spaces << "\r";

    std::string message = "";

    float       percentDone = 0;
    if (m_fPercentage >= 0)
        percentDone = m_fPercentage;
    else
        percentDone = (float)m_iSteps / (float)m_iMaxSteps;

    const auto BARWIDTH = std::clamp(w.ws_col - static_cast<unsigned long>(m_szCurrentMessage.length()) - 2, 0UL, 50UL);

    // draw bar
    message += std::string{" "} + Colors::GREEN;
    size_t i = 0;
    for (; i < std::floor(percentDone * BARWIDTH); ++i) {
        message += "━";
    }

    if (i < BARWIDTH) {
        i++;

        message += std::string{"╍"} + Colors::RESET;

        for (; i < BARWIDTH; ++i) {
            message += "━";
        }
    } else
        message += Colors::RESET;

    // draw progress
    if (m_fPercentage >= 0)
        message += "  " + std::format("{}%", static_cast<int>(percentDone * 100.0)) + " ";
    else
        message += "  " + std::format("{} / {}", m_iSteps, m_iMaxSteps) + " ";

    // draw message
    std::cout << message + " " + m_szCurrentMessage;

    std::fflush(stdout);
}