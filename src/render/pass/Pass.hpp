#pragma once

#include "../../defines.hpp"
#include "PassElement.hpp"

class CRenderPass {
  public:
    bool empty() const;
    bool single() const;
    bool needsIntrospection() const;

    void add(SP<IPassElement> elem);
    void clear();

    void render(const CRegion& damage_);

  private:
    CRegion damage;

    struct SPassElementData {
        CRegion          elementDamage;
        SP<IPassElement> element;
    };

    std::vector<SP<SPassElementData>> m_vPassElements;

    SP<IPassElement>                  currentPassInfo = nullptr;

    void                              simplify();

    friend class CHyprOpenGLImpl;
};