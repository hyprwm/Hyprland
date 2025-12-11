#include "Zoom.hpp"

#include <hyprlang.hpp>
#include "../config/ConfigValue.hpp"
#include "../managers/input/InputManager.hpp"
#include "../render/OpenGL.hpp"
#include "desktop/DesktopTypes.hpp"
#include "render/Renderer.hpp"

void zoomWithDetachedCamera(CBox& result, CMonitorZoomController* zc, const SCurrentRenderData& m_renderData) {
    const auto M       = m_renderData.pMonitor;
    auto       monbox  = CBox(0, 0, M->m_size.x, M->m_size.y);
    const auto ZOOM    = m_renderData.mouseZoomFactor;
    const auto CAMERAW = monbox.w / ZOOM;
    const auto CAMERAH = monbox.h / ZOOM;
    const auto MOUSE   = g_pInputManager->getMouseCoordsInternal() - M->m_position;

    if (zc->m_lastZoomLevel != ZOOM) {
        if (zc->m_resetCameraState) {
            zc->m_resetCameraState = false;
            zc->m_camera           = CBox(0, 0, M->m_size.x, M->m_size.y);
            zc->m_lastZoomLevel    = 1.0f;
        }
        const CBox old = zc->m_camera;

        // mouse normalized inside screen (0..1)
        const float mx = MOUSE.x / M->m_size.x;
        const float my = MOUSE.y / M->m_size.y;
        // world-space point under the cursor before zoom
        const float mouseWorldX = old.x + (mx * old.w);
        const float mouseWorldY = old.y + (my * old.h);
        // compute new top-left so the same world point stays under the cursor
        const float newX = mouseWorldX - (mx * CAMERAW);
        const float newY = mouseWorldY - (my * CAMERAH);

        zc->m_camera = CBox(newX, newY, CAMERAW, CAMERAH);
        // Detect if this zoom would've caused jerk to keep mouse in view and disable edges if so
        if (!zc->m_camera.copy().scaleFromCenter(.9).containsPoint(MOUSE))
            zc->m_padCamEdges = false;
        zc->m_lastZoomLevel = ZOOM;
    }

    // Keep mouse inside cameraview
    auto smallerbox = zc->m_camera;
    // Prevent zoom step from causing us to jerk to keep mouse in padded camera view,
    // but let us switch to the padded camera once the mouse moves into the safe area
    if (!zc->m_padCamEdges)
        if (smallerbox.copy().scaleFromCenter(.9).containsPoint(MOUSE))
            zc->m_padCamEdges = true;
    if (zc->m_padCamEdges)
        smallerbox.scaleFromCenter(.9);
    if (!smallerbox.containsPoint(MOUSE)) {
        if (MOUSE.x < smallerbox.x)
            zc->m_camera.x -= smallerbox.x - MOUSE.x;
        if (MOUSE.y < smallerbox.y)
            zc->m_camera.y -= smallerbox.y - MOUSE.y;
        if (MOUSE.y > smallerbox.y + smallerbox.h)
            zc->m_camera.y += MOUSE.y - (smallerbox.y + smallerbox.h);
        if (MOUSE.x > smallerbox.x + smallerbox.w)
            zc->m_camera.x += MOUSE.x - (smallerbox.x + smallerbox.w);
    }

    auto z = ZOOM * M->m_scale;
    monbox.scale(z).translate(-zc->m_camera.pos() * z);

    result = monbox;
}

void CMonitorZoomController::applyZoomTransform(CBox& monbox, const SCurrentRenderData& m_renderData) {
    static auto PZOOMRIGID          = CConfigValue<Hyprlang::INT>("cursor:zoom_rigid");
    static auto PZOOMDETACHEDCAMERA = CConfigValue<Hyprlang::INT>("cursor:zoom_detached_camera");

    const auto  ORIGINAL = monbox;
    const auto  MONITOR  = m_renderData.pMonitor;
    const auto  INITANIM = MONITOR->m_zoomAnimProgress->value() != 1.0;
    const auto  ZOOM     = m_renderData.mouseZoomFactor;

    if (ZOOM != 1.0f) {
        if (*PZOOMDETACHEDCAMERA && !INITANIM) {
            zoomWithDetachedCamera(monbox, this, m_renderData);
        } else {
            const auto ZOOMCENTER =
                m_renderData.mouseZoomUseMouse ? (g_pInputManager->getMouseCoordsInternal() - MONITOR->m_position) * MONITOR->m_scale : MONITOR->m_transformedSize / 2.f;

            monbox.translate(-ZOOMCENTER).scale(ZOOM).translate(*PZOOMRIGID ? MONITOR->m_transformedSize / 2.0 : ZOOMCENTER);
        }

        monbox.x = std::min(monbox.x, 0.0);
        monbox.y = std::min(monbox.y, 0.0);
        if (monbox.x + monbox.width < ORIGINAL.w)
            monbox.x = ORIGINAL.w - monbox.width;
        if (monbox.y + monbox.height < ORIGINAL.h)
            monbox.y = ORIGINAL.h - monbox.height;
    }
}
