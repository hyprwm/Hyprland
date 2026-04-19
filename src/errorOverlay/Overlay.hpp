#pragma once

#include <vector>

#include "../defines.hpp"
#include "../render/Texture.hpp"
#include "../helpers/AnimatedVariable.hpp"
#include "../helpers/MiscFunctions.hpp"
#include "../config/shared/complex/ComplexDataTypes.hpp"

namespace ErrorOverlay {

    namespace Colors {
        constexpr const float                   ANGLE_30 = 0.52359877;

        static const Config::CGradientValueData ERROR =
            Config::CGradientValueData{std::vector<CHyprColor>{*configStringToInt("0xffff6666"), *configStringToInt("0xff800000")}, ANGLE_30};
        static const Config::CGradientValueData WARNING =
            Config::CGradientValueData{std::vector<CHyprColor>{*configStringToInt("0xffffdb4d"), *configStringToInt("0xff665200")}, ANGLE_30};
    };

    class COverlay {
      public:
        COverlay();
        ~COverlay() = default;

        void  queueCreate(std::string message, const CHyprColor& color);
        void  queueCreate(std::string message, const Config::CGradientValueData& gradient);
        void  queueError(std::string err);
        void  draw();
        void  destroy();

        bool  active();
        float height(); // logical

      private:
        void                       createQueued();
        std::string                m_queued = "";
        Config::CGradientValueData m_queuedBorderGradient;
        Config::CGradientValueData m_borderGradient;
        bool                       m_queuedDestroy = false;
        bool                       m_isCreated     = false;
        SP<Render::ITexture>       m_textTexture;
        Vector2D                   m_textSize    = {};
        float                      m_outerPad    = 0.F;
        float                      m_radius      = 0.F;
        float                      m_textOffsetX = 0.F;
        float                      m_textOffsetY = 0.F;
        PHLANIMVAR<float>          m_fadeOpacity;
        CBox                       m_damageBox  = {0, 0, 0, 0};
        float                      m_lastHeight = 0.F;

        bool                       m_monitorChanged = false;
    };

    UP<COverlay>& overlay();
}
