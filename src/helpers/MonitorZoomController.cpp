#include "MonitorZoomController.hpp"

#include <hyprlang.hpp>
#include "../config/ConfigValue.hpp"
#include "../managers/input/InputManager.hpp"
#include "../render/OpenGL.hpp"
#include "desktop/DesktopTypes.hpp"
#include "render/Renderer.hpp"

void CMonitorZoomController::zoomWithDetachedCamera(CBox& result, const SCurrentRenderData& m_renderData) {
    const auto m      = m_renderData.pMonitor;
    auto       monbox = CBox(0, 0, m->m_size.x, m->m_size.y);
    const auto ZOOM   = m_renderData.mouseZoomFactor;
    const auto MOUSE  = g_pInputManager->getMouseCoordsInternal() - m->m_position;

    if (m_lastZoomLevel != ZOOM) {
        if (m_resetCameraState) {
            m_resetCameraState = false;
            m_camera           = CBox(0, 0, m->m_size.x, m->m_size.y);
            m_lastZoomLevel    = 1.0f;
        }
        const CBox old = m_camera;

        // mouse normalized inside screen (0..1)
        const float mx = MOUSE.x / m->m_size.x;
        const float my = MOUSE.y / m->m_size.y;
        // world-space point under the cursor before zoom
        const float mouseWorldX = old.x + (mx * old.w);
        const float mouseWorldY = old.y + (my * old.h);

        const auto  CAMERAW = monbox.w / ZOOM;
        const auto  CAMERAH = monbox.h / ZOOM;

        // compute new top-left so the same world point stays under the cursor
        const float newX = mouseWorldX - (mx * CAMERAW);
        const float newY = mouseWorldY - (my * CAMERAH);

        m_camera = CBox(newX, newY, CAMERAW, CAMERAH);
        // Detect if this zoom would've caused jerk to keep mouse in view and disable edges if so
        if (!m_camera.copy().scaleFromCenter(.9).containsPoint(MOUSE))
            m_padCamEdges = false;
        m_lastZoomLevel = ZOOM;
    }

    // Keep mouse inside cameraview
    auto smallerbox = m_camera;
    // Prevent zoom step from causing us to jerk to keep mouse in padded camera view,
    // but let us switch to the padded camera once the mouse moves into the safe area
    if (!m_padCamEdges)
        if (smallerbox.copy().scaleFromCenter(.9).containsPoint(MOUSE))
            m_padCamEdges = true;
    if (m_padCamEdges)
        smallerbox.scaleFromCenter(.9);
    if (!smallerbox.containsPoint(MOUSE)) {
        if (MOUSE.x < smallerbox.x)
            m_camera.x -= smallerbox.x - MOUSE.x;
        if (MOUSE.y < smallerbox.y)
            m_camera.y -= smallerbox.y - MOUSE.y;
        if (MOUSE.y > smallerbox.y + smallerbox.h)
            m_camera.y += MOUSE.y - (smallerbox.y + smallerbox.h);
        if (MOUSE.x > smallerbox.x + smallerbox.w)
            m_camera.x += MOUSE.x - (smallerbox.x + smallerbox.w);
    }

    auto z = ZOOM * m->m_scale;
    monbox.scale(z).translate(-m_camera.pos() * z);

    result = monbox;
}

void CMonitorZoomController::applyZoomTransform(CBox& monbox, const SCurrentRenderData& m_renderData) {
    static auto PZOOMRIGID          = CConfigValue<Hyprlang::INT>("cursor:zoom_rigid");
    static auto PZOOMDETACHEDCAMERA = CConfigValue<Hyprlang::INT>("cursor:zoom_detached_camera");
    const auto  ZOOM                = m_renderData.mouseZoomFactor;

    if (ZOOM == 1.0f)
        return;

    const auto m        = m_renderData.pMonitor;
    const auto ORIGINAL = monbox;
    const auto INITANIM = m->m_zoomAnimProgress->value() != 1.0;

    if (*PZOOMDETACHEDCAMERA && !INITANIM)
        zoomWithDetachedCamera(monbox, m_renderData);
    else {
        const auto ZOOMCENTER = m_renderData.mouseZoomUseMouse ? (g_pInputManager->getMouseCoordsInternal() - m->m_position) * m->m_scale : m->m_transformedSize / 2.f;

        monbox.translate(-ZOOMCENTER).scale(ZOOM).translate(*PZOOMRIGID ? m->m_transformedSize / 2.0 : ZOOMCENTER);
    }

    monbox.x = std::min(monbox.x, 0.0);
    monbox.y = std::min(monbox.y, 0.0);
    if (monbox.x + monbox.width < ORIGINAL.w)
        monbox.x = ORIGINAL.w - monbox.width;
    if (monbox.y + monbox.height < ORIGINAL.h)
        monbox.y = ORIGINAL.h - monbox.height;
}
