#pragma once

#include "PassElement.hpp"
#include "../BatchManager.hpp"
#include <vector>

class CHyprDropShadowDecoration;

class CBatchedShadowPassElement : public IPassElement {
  public:
    struct SShadowBatch {
        CHyprDropShadowDecoration* deco  = nullptr;
        float                      alpha = 1.0f;
    };

    CBatchedShadowPassElement()          = default;
    virtual ~CBatchedShadowPassElement() = default;

    virtual void draw(const CRegion& damage) override;
    virtual bool needsLiveBlur() override {
        return false;
    }
    virtual bool needsPrecomputeBlur() override {
        return false;
    }

    virtual const char* passName() override {
        return "CBatchedShadowPassElement";
    }

    void   addShadow(CHyprDropShadowDecoration* deco, float alpha);
    size_t getShadowCount() const {
        return m_shadows.size();
    }

  private:
    std::vector<SShadowBatch> m_shadows;
};