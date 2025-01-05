#pragma once

#include "../../defines.hpp"
#include "PassElement.hpp"

class CGradientValueData;
class CTexture;

class CRenderPass {
  public:
    bool    empty() const;
    bool    single() const;
    bool    needsIntrospection() const;

    void    add(SP<IPassElement> elem);
    void    clear();
    void    removeAllOfType(const std::string& type);

    CRegion render(const CRegion& damage_);

  private:
    CRegion              damage;
    std::vector<CRegion> occludedRegions;
    CRegion              totalLiveBlurRegion;

    struct SPassElementData {
        CRegion          elementDamage;
        SP<IPassElement> element;
        bool             discard = false;
    };

    std::vector<SP<SPassElementData>> m_vPassElements;

    SP<IPassElement>                  currentPassInfo = nullptr;

    void                              simplify();
    float                             oneBlurRadius();
    void                              renderDebugData();

    struct {
        bool         present = false;
        SP<CTexture> keyboardFocusText, pointerFocusText, lastWindowText;
    } debugData;

    friend class CHyprOpenGLImpl;
};
