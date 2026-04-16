#include "Parser.hpp"

#include "../../../debug/log/Logger.hpp"

#include <hyprutils/string/VarList2.hpp>
#include <hyprutils/string/Numeric.hpp>
#include <hyprutils/string/String.hpp>
#include <string>
#include <algorithm>

using namespace Config;
using namespace Hyprutils::String;

static bool parseModeLine(const std::string& modeline, drmModeModeInfo& mode) {
    auto args = CVarList2(std::string{modeline}, 0, 's');

    auto keyword = std::string{args[0]};
    std::ranges::transform(keyword, keyword.begin(), ::tolower);

    if (keyword != "modeline")
        return false;

    if (args.size() < 10) {
        Log::logger->log(Log::ERR, "modeline parse error: expected at least 9 arguments, got {}", args.size() - 1);
        return false;
    }

    int  argno = 1;

    auto huErrStr = [](Hyprutils::String::eNumericParseResult r) -> const char* {
        switch (r) {
            case Hyprutils::String::NUMERIC_PARSE_BAD: return "bad input";
            case Hyprutils::String::NUMERIC_PARSE_GARBAGE: return "garbage input";
            case Hyprutils::String::NUMERIC_PARSE_OUT_OF_RANGE: return "out of range";
            case Hyprutils::String::NUMERIC_PARSE_OK: return "ok";
            default: return "error";
        }
    };

#define ASSIGN_OR_FAIL(prop, type)                                                                                                                                                 \
    if (auto n = strToNumber<type>(args[argno++]); n)                                                                                                                              \
        prop = n.value();                                                                                                                                                          \
    else {                                                                                                                                                                         \
        Log::logger->log(Log::ERR, "modeline parse error: invalid input at \"{}\": {}", args[argno - 1], huErrStr(n.error()));                                                     \
        return false;                                                                                                                                                              \
    }

    mode.type = DRM_MODE_TYPE_USERDEF;

    ASSIGN_OR_FAIL(mode.clock, float);
    ASSIGN_OR_FAIL(mode.hdisplay, int);
    ASSIGN_OR_FAIL(mode.hsync_start, int);
    ASSIGN_OR_FAIL(mode.hsync_end, int);
    ASSIGN_OR_FAIL(mode.htotal, int);
    ASSIGN_OR_FAIL(mode.vdisplay, int);
    ASSIGN_OR_FAIL(mode.vsync_start, int);
    ASSIGN_OR_FAIL(mode.vsync_end, int);
    ASSIGN_OR_FAIL(mode.vtotal, int);

    mode.clock *= 1000;
    mode.vrefresh = mode.clock * 1000.0 * 1000.0 / mode.htotal / mode.vtotal;

#undef ASSIGN_OR_FAIL

    // clang-format off
    static std::unordered_map<std::string, uint32_t> flagsmap = {
        {"+hsync", DRM_MODE_FLAG_PHSYNC},
        {"-hsync", DRM_MODE_FLAG_NHSYNC},
        {"+vsync", DRM_MODE_FLAG_PVSYNC},
        {"-vsync", DRM_MODE_FLAG_NVSYNC},
        {"Interlace", DRM_MODE_FLAG_INTERLACE},
    };
    // clang-format on

    for (; argno < sc<int>(args.size()); argno++) {
        auto key = std::string{args[argno]};
        std::ranges::transform(key, key.begin(), ::tolower);

        auto it = flagsmap.find(key);

        if (it != flagsmap.end())
            mode.flags |= it->second;
        else
            Log::logger->log(Log::ERR, "Invalid flag {} in modeline", key);
    }

    snprintf(mode.name, sizeof(mode.name), "%dx%d@%d", mode.hdisplay, mode.vdisplay, mode.vrefresh / 1000);

    return true;
}

CMonitorRuleParser::CMonitorRuleParser(const std::string& name) {
    m_rule.m_name = name;
}

const std::string& CMonitorRuleParser::name() {
    return m_rule.m_name;
}

Config::CMonitorRule& CMonitorRuleParser::rule() {
    return m_rule;
}

std::optional<std::string> CMonitorRuleParser::getError() {
    if (m_error.empty())
        return {};
    return m_error;
}

bool CMonitorRuleParser::parseMode(const std::string& value) {
    if (value.starts_with("pref"))
        m_rule.m_resolution = Vector2D();
    else if (value.starts_with("highrr"))
        m_rule.m_resolution = Vector2D(-1, -1);
    else if (value.starts_with("highres"))
        m_rule.m_resolution = Vector2D(-1, -2);
    else if (value.starts_with("maxwidth"))
        m_rule.m_resolution = Vector2D(-1, -3);
    else if (parseModeLine(value, m_rule.m_drmMode)) {
        m_rule.m_resolution  = Vector2D(m_rule.m_drmMode.hdisplay, m_rule.m_drmMode.vdisplay);
        m_rule.m_refreshRate = sc<float>(m_rule.m_drmMode.vrefresh) / 1000;
    } else {

        if (!value.contains("x")) {
            m_error += "invalid resolution ";
            m_rule.m_resolution = Vector2D();
            return false;
        } else {
            try {
                m_rule.m_resolution.x = stoi(value.substr(0, value.find_first_of('x')));
                m_rule.m_resolution.y = stoi(value.substr(value.find_first_of('x') + 1, value.find_first_of('@')));

                if (value.contains("@"))
                    m_rule.m_refreshRate = stof(value.substr(value.find_first_of('@') + 1));
            } catch (...) {
                m_error += "invalid resolution ";
                m_rule.m_resolution = Vector2D();
                return false;
            }
        }
    }
    return true;
}

bool CMonitorRuleParser::parsePosition(const std::string& value, bool isFirst) {
    if (value.starts_with("auto")) {
        m_rule.m_offset = Vector2D(-INT32_MAX, -INT32_MAX);
        // If this is the first monitor rule needs to be on the right.
        if (value == "auto-right" || value == "auto" || isFirst)
            m_rule.m_autoDir = eAutoDirs::DIR_AUTO_RIGHT;
        else if (value == "auto-left")
            m_rule.m_autoDir = eAutoDirs::DIR_AUTO_LEFT;
        else if (value == "auto-up")
            m_rule.m_autoDir = eAutoDirs::DIR_AUTO_UP;
        else if (value == "auto-down")
            m_rule.m_autoDir = eAutoDirs::DIR_AUTO_DOWN;
        else if (value == "auto-center-right")
            m_rule.m_autoDir = eAutoDirs::DIR_AUTO_CENTER_RIGHT;
        else if (value == "auto-center-left")
            m_rule.m_autoDir = eAutoDirs::DIR_AUTO_CENTER_LEFT;
        else if (value == "auto-center-up")
            m_rule.m_autoDir = eAutoDirs::DIR_AUTO_CENTER_UP;
        else if (value == "auto-center-down")
            m_rule.m_autoDir = eAutoDirs::DIR_AUTO_CENTER_DOWN;
        else {
            Log::logger->log(Log::WARN,
                             "Invalid auto direction. Valid options are 'auto',"
                             "'auto-up', 'auto-down', 'auto-left', 'auto-right',"
                             "'auto-center-up', 'auto-center-down',"
                             "'auto-center-left', and 'auto-center-right'.");
            m_error += "invalid auto direction ";
            return false;
        }
    } else {
        if (!value.contains("x")) {
            m_error += "invalid offset ";
            m_rule.m_offset = Vector2D(-INT32_MAX, -INT32_MAX);
            return false;
        } else {
            try {
                m_rule.m_offset.x = stoi(value.substr(0, value.find_first_of('x')));
                m_rule.m_offset.y = stoi(value.substr(value.find_first_of('x') + 1));
            } catch (...) {
                m_error += "invalid offset ";
                m_rule.m_offset = Vector2D(-INT32_MAX, -INT32_MAX);
                return false;
            }
        }
    }
    return true;
}

bool CMonitorRuleParser::parseScale(const std::string& value) {
    if (value.starts_with("auto"))
        m_rule.m_scale = -1;
    else {
        if (!isNumber(value, true)) {
            m_error += "invalid scale ";
            return false;
        } else {
            m_rule.m_scale = stof(value);

            if (m_rule.m_scale < 0.25F) {
                m_error += "invalid scale ";
                m_rule.m_scale = 1;
                return false;
            }
        }
    }
    return true;
}

bool CMonitorRuleParser::parseTransform(const std::string& value) {
    if (!isNumber(value)) {
        m_error += "invalid transform ";
        return false;
    }

    const auto TSF = std::stoi(value);
    if (std::clamp(TSF, 0, 7) != TSF) {
        Log::logger->log(Log::ERR, "Invalid transform {} in monitor", TSF);
        m_error += "invalid transform ";
        return false;
    }
    m_rule.m_transform = sc<wl_output_transform>(TSF);
    return true;
}

bool CMonitorRuleParser::parseBitdepth(const std::string& value) {
    m_rule.m_enable10bit = value == "10";
    return true;
}

bool CMonitorRuleParser::parseCM(const std::string& value) {
    auto parsedCM = NCMType::fromString(value);
    if (!parsedCM.has_value()) {
        m_error += "invalid cm ";
        return false;
    }
    m_rule.m_cmType = parsedCM.value();
    return true;
}

bool CMonitorRuleParser::parseSDRBrightness(const std::string& value) {
    try {
        m_rule.m_sdrBrightness = stof(value);
    } catch (...) {
        m_error += "invalid sdrbrightness ";
        return false;
    }
    return true;
}

bool CMonitorRuleParser::parseSDRSaturation(const std::string& value) {
    try {
        m_rule.m_sdrSaturation = stof(value);
    } catch (...) {
        m_error += "invalid sdrsaturation ";
        return false;
    }
    return true;
}

bool CMonitorRuleParser::parseVRR(const std::string& value) {
    if (!isNumber(value)) {
        m_error += "invalid vrr ";
        return false;
    }

    m_rule.m_vrr = std::stoi(value);
    return true;
}

bool CMonitorRuleParser::parseVrrMinHz(const std::string& value) {
    if (!isNumber(value)) {
        m_error += "invalid vrr_min_hz ";
        return false;
    }

    const int hz = std::stoi(value);
    if (hz <= 0) {
        m_error += "invalid vrr_min_hz ";
        return false;
    }
    m_rule.m_vrrMinHz = hz;
    return true;
}

bool CMonitorRuleParser::parseICC(const std::string& val) {
    if (val.empty()) {
        m_error += "invalid icc ";
        return false;
    }
    m_rule.m_iccFile = val;
    return true;
}

void CMonitorRuleParser::setDisabled() {
    m_rule.m_disabled = true;
}

void CMonitorRuleParser::setMirror(const std::string& value) {
    m_rule.m_mirrorOf = value;
}

bool CMonitorRuleParser::setReserved(const Desktop::CReservedArea& value) {
    m_rule.m_reservedArea = value;
    return true;
}