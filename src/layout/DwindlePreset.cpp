#include "DwindlePreset.hpp"
#include "../debug/Log.hpp"
#include <hyprutils/string/ConstVarList.hpp>

using namespace Hyprutils::String;

SP<CDwindlePreset> CDwindlePreset::create(const std::string& dataStr) {

    CConstVarList data(dataStr);

    if (data[0] != "dwindle") {
        Debug::log(LOG, "CDwindlePreset: Skipping preset, not for dwindle");
        return nullptr;
    }

    if (data[1].empty()) {
        Debug::log(LOG, "CDwindlePreset: Skipping preset, no name");
        return nullptr;
    }

    SP<CDwindlePreset> preset = SP<CDwindlePreset>(new CDwindlePreset(std::string{data[1]}));

    // start parsing chunks

    for (size_t i = 2; i < data.size(); ++i) {
        if (!preset->addChunk(data[i]))
            return nullptr;
    }

    return preset;
}

bool CDwindlePreset::addChunk(const std::string_view& dataStr) {

    CConstVarList          data(std::string{dataStr}, 0, 's');

    SDwindlePresetNodeData chunk;

    size_t                 step           = 0;
    std::string            windowSelector = "";

    for (const auto& d : data) {
        switch (step) {
            case 0: {
                // directions
                if (d == "up") {
                    chunk.moves.emplace_back(PRESET_MOVE_UP);
                    break;
                } else if (d == "left") {
                    chunk.moves.emplace_back(PRESET_MOVE_LEFT);
                    break;
                } else if (d == "right") {
                    chunk.moves.emplace_back(PRESET_MOVE_RIGHT);
                    break;
                } else if (d == "root") {
                    // special treatment to select root
                    step       = 4;
                    chunk.root = true;
                    break;
                }

                // not a direction, let's move to step 2
            }
                [[fallthrough]];
            case 1: {
                // split direction
                if (d == "horizontal" || d == "horiz")
                    chunk.splitHorizontal = true;
                else if (d == "vertical" || d == "vert")
                    chunk.splitHorizontal = false;
                else {
                    Debug::log(ERR, "CDwindlePreset::addChunk: failed to parse chunk \"{}\", no direction", dataStr);
                    return false;
                }
                step = 2;
                break;
            }

            case 2: {
                // split width
                float perc = 100.F;
                try {
                    perc = d.ends_with('%') ? std::stoi(std::string{d.substr(0, d.length() - 1)}) : std::stoi(std::string{d});
                } catch (...) {
                    Debug::log(ERR, "CDwindlePreset::addChunk: failed to parse chunk \"{}\", no percentage", dataStr);
                    return false;
                }

                if (perc <= 1 || perc >= 99) {
                    Debug::log(ERR, "CDwindlePreset::addChunk: failed to parse chunk \"{}\", invalid percentage", dataStr);
                    return false;
                }

                chunk.splitRatio = perc / 50.F;
                step             = 3;
                break;
            }

            case 3: {
                // split direction
                if (d == "left")
                    chunk.splitRight = false;
                else if (d == "right")
                    chunk.splitRight = true;
                else {
                    Debug::log(ERR, "CDwindlePreset::addChunk: failed to parse chunk \"{}\", no split direction", dataStr);
                    return false;
                }

                step = 4;

                break;
            }

            case 4: {

                // add to window selector
                windowSelector += d;
                windowSelector += " ";

                break;
            }

            default: {
                Debug::log(ERR, "CDwindlePreset::addChunk: failed to parse chunk \"{}\", trailing data", dataStr);
                return false;
            }
        }
    }

    if (!windowSelector.empty())
        windowSelector.pop_back();

    chunk.windowSelector = windowSelector;

    if (m_data.empty() && !chunk.root)
        m_data.emplace_back(SDwindlePresetNodeData{.root = true});

    m_data.emplace_back(std::move(chunk));

    return true;
}

const std::vector<CDwindlePreset::SDwindlePresetNodeData>& CDwindlePreset::data() {
    return m_data;
}
