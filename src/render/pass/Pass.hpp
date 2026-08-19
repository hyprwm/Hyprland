#pragma once

#include "../../defines.hpp"
#include "PassElement.hpp"

class CGradientValueData;

namespace Render {
    class ITexture;
    class CRenderingContext;

    class CRenderPass {
      public:
        bool    empty() const;
        bool    single() const;
        bool    needsLiveBlur(const CRenderingContext& context);
        bool    needsPrecomputeBlur(const CRenderingContext& context);

        void    add(UP<IPassElement>&& elem);
        void    clear();
        void    removeAllOfType(const std::string& type);

        CRegion render(CRenderingContext& context, const CRegion& damage_);

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

        void                          simplify(CRenderingContext& context, bool willBlur, const CRegion& liveBlurRegion);
        void                          planBackdropScopes(const CRenderingContext& context);
        void                          renderDebugData(CRenderingContext& context);

        struct {
            bool         present = false;
            SP<ITexture> keyboardFocusText, pointerFocusText, lastWindowText;
        } m_debugData;
    };
}
