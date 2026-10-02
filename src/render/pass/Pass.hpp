#pragma once

#include "../../defines.hpp"
#include "PassElement.hpp"

class CGradientValueData;

namespace Render {
    class ITexture;

    class CRenderPass {
      public:
        bool    empty() const;
        bool    single() const;
        bool    needsLiveBlur(CRenderContext& ctx);
        bool    needsPrecomputeBlur(CRenderContext& ctx);

        void    add(UP<IPassElement>&& elem);
        void    clear();
        void    removeAllOfType(const std::string& type);

        CRegion render(CRenderContext& ctx, const CRegion& damage_);

      private:
        CRegion              m_damage;
        std::vector<CRegion> m_occludedRegions;
        CRegion              m_totalLiveBlurRegion;

        struct SPassElementData {
            CRegion          elementDamage;
            UP<IPassElement> element;
            bool             discard = false;
        };

        std::vector<SPassElementData> m_passElements;

        void                          simplify(CRenderContext& ctx, bool willBlur, const CRegion& liveBlurRegion);
        void                          planBackdropScopes(CRenderContext& ctx);
        void                          renderDebugData(CRenderContext& ctx);

        struct {
            bool         present = false;
            SP<ITexture> keyboardFocusText, pointerFocusText, lastWindowText;
        } m_debugData;
    };
}
