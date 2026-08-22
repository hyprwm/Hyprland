#include "BoxShadowPassElement.hpp"
#include "../Renderer.hpp"

CBoxShadowPassElement::CBoxShadowPassElement(const SBoxShadowData& data) : CShadowPassElement(SShadowData{}), m_boxData(data) {
    ;
}

std::optional<CBox> CBoxShadowPassElement::boundingBox(const Render::CRenderingContext& context) {
    if (!context.sceneMonitor)
        return std::nullopt;

    return m_boxData.box.copy().scale(1.F / context.sceneMonitor->m_scale).round();
}
