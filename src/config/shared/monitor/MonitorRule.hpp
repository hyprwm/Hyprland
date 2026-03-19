#pragma once

#include <string>
#include <optional>
#include <xf86drmMode.h>

#include "../../../helpers/math/Math.hpp"
#include "../../../helpers/cm/ColorManagement.hpp"
#include "../../../helpers/CMType.hpp"
#include "../../../helpers/TransferFunction.hpp"
#include "../../../desktop/reserved/ReservedArea.hpp"

namespace Config {
    // Enum for the different types of auto directions, e.g. auto-left, auto-up.
    enum eAutoDirs : uint8_t {
        DIR_AUTO_NONE = 0, /* None will be treated as right. */
        DIR_AUTO_UP,
        DIR_AUTO_DOWN,
        DIR_AUTO_LEFT,
        DIR_AUTO_RIGHT,
        DIR_AUTO_CENTER_UP,
        DIR_AUTO_CENTER_DOWN,
        DIR_AUTO_CENTER_LEFT,
        DIR_AUTO_CENTER_RIGHT
    };

    class CMonitorRule {
      public:
        CMonitorRule()  = default;
        ~CMonitorRule() = default;

        eAutoDirs              m_autoDir       = DIR_AUTO_NONE;
        std::string            m_name          = "";
        Vector2D               m_resolution    = Vector2D(1280, 720);
        Vector2D               m_offset        = Vector2D(0, 0);
        float                  m_scale         = 1;
        float                  m_refreshRate   = 60; // Hz
        bool                   m_disabled      = false;
        wl_output_transform    m_transform     = WL_OUTPUT_TRANSFORM_NORMAL;
        std::string            m_mirrorOf      = "";
        bool                   m_enable10bit   = false;
        NCMType::eCMType       m_cmType        = NCMType::CM_SRGB;
        NTransferFunction::eTF m_sdrEotf       = NTransferFunction::TF_DEFAULT;
        float                  m_sdrSaturation = 1.F; // SDR -> HDR
        float                  m_sdrBrightness = 1.F; // SDR -> HDR
        Desktop::CReservedArea m_reservedArea;
        std::string            m_iccFile;

        int                    m_supportsWideColor = 0;    // 0 - auto, 1 - force enable, -1 - force disable
        int                    m_supportsHDR       = 0;    // 0 - auto, 1 - force enable, -1 - force disable
        float                  m_sdrMinLuminance   = 0.2F; // SDR -> HDR
        int                    m_sdrMaxLuminance   = 80;   // SDR -> HDR

        // Incorrect values will result in reduced luminance range or incorrect tonemapping. Shouldn't damage the HW. Use with care in case of a faulty monitor firmware.
        float              m_minLuminance    = -1.F; // >= 0 overrides EDID
        int                m_maxLuminance    = -1;   // >= 0 overrides EDID
        int                m_maxAvgLuminance = -1;   // >= 0 overrides EDID

        drmModeModeInfo    m_drmMode = {};
        std::optional<int> m_vrr;
    };
};