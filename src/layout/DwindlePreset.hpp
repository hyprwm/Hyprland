#pragma once

#include <string_view>
#include <string>
#include <vector>
#include <cstdint>
#include "../helpers/memory/Memory.hpp"

class CDwindlePreset {
  public:
    // can return nullptr if failed or not for dwindle
    static SP<CDwindlePreset> create(const std::string& data);
    ~CDwindlePreset() = default;

    enum eMoveDirection : uint8_t {
        PRESET_MOVE_UP = 0,
        PRESET_MOVE_LEFT,
        PRESET_MOVE_RIGHT,
    };

    struct SDwindlePresetNodeData {
        bool                        splitHorizontal = true, splitRight = true;
        float                       splitRatio = 1.F;
        std::vector<eMoveDirection> moves;
    };

    const std::vector<SDwindlePresetNodeData>& data();

    const std::string                          m_name = "";

  private:
    CDwindlePreset(const std::string& name) : m_name(name) {}

    std::vector<SDwindlePresetNodeData> m_data;

    bool                                addChunk(const std::string_view& data);
};