#pragma once

#include "PassElement.hpp"
#include "../BatchManager.hpp"
#include <vector>
#include <memory>

class CBatchedPassElement : public IPassElement {
  public:
    struct SBatchableElement {
        enum Type {
            RECT,
            TEXTURE,
            SHADOW
        } type;
        
        CBox             box;
        CHyprColor       color;
        int              round;
        float            roundingPower;
        float            alpha;
        SP<CTexture>     texture;
        int              shadowRange;
    };

    CBatchedPassElement();
    virtual ~CBatchedPassElement() = default;

    virtual void             draw(const CRegion& damage) override;
    virtual bool             needsPrecomputeBlur() override;

    void                     addElement(UP<IPassElement> element);
    void                     addBatchableElement(const SBatchableElement& element);
    void                     clear();
    
    static bool              canBatch(IPassElement* element);

  private:
    std::vector<UP<IPassElement>>  m_unbatchableElements;
    std::vector<SBatchableElement> m_batchableElements;
    
    void                           extractBatchableData(IPassElement* element);
};