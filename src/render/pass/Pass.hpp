#pragma once

#include "../../defines.hpp"
#include "PassElement.hpp"

class CGradientValueData;

class CRenderPass {
  public:
    bool    empty() const;
    bool    single() const;
    bool    needsIntrospection() const;

    void    add(SP<IPassElement> elem);
    void    clear();

    CRegion render(const CRegion& damage_);

  private:
    CRegion damage;

    struct SPassElementData {
        CRegion          elementDamage;
        SP<IPassElement> element;
        bool             discard = false;
    };

    std::vector<SP<SPassElementData>> m_vPassElements;
    std::vector<SP<SPassElementData>> m_vDiscardedPassElements;

    SP<IPassElement>                  currentPassInfo = nullptr;

    void                              simplify();
    float                             oneBlurRadius();

    friend class CHyprOpenGLImpl;
};
