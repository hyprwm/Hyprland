#pragma once
#include "PassElement.hpp"

class CHyprDropShadowDecoration;

class CShadowPassElement : public IPassElement {
  public:
    struct SShadowData {
        CHyprDropShadowDecoration* deco = nullptr;
        float                      a    = 1.F;
    };

    CShadowPassElement(const SShadowData& data_);
    virtual ~CShadowPassElement() = default;

    virtual void        draw(const CRegion& damage);
    virtual bool        needsLiveBlur();
    virtual bool        needsPrecomputeBlur();

    virtual const char* passName() {
        return "CShadowPassElement";
    }

  private:
    SShadowData m_data;
};