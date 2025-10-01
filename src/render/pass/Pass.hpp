#pragma once

#include "../../defines.hpp"
#include "PassElement.hpp"

class CGradientValueData;
class CTexture;

class CRenderPass {
  public:
    bool    empty() const;
    bool    single() const;

    void    add(SP<IPassElement> elem);
    void    clear();
    void    removeAllOfType(const std::string& type);

    CRegion render(const CRegion& damage_);

  private:
    CRegion              m_damage;
    std::vector<CRegion> m_occludedRegions;
    CRegion              m_totalLiveBlurRegion;

    struct SPassElementData {
        CRegion          elementDamage;
        SP<IPassElement> element;
        bool             discard = false;
    };

    std::vector<SP<SPassElementData>> m_passElements;

    void                              simplify();
    float                             oneBlurRadius();
    void                              renderDebugData();

    struct {
        bool         present = false;
        SP<CTexture> keyboardFocusText, pointerFocusText, lastWindowText;
    } m_debugData;

    friend class CHyprOpenGLImpl;
};
