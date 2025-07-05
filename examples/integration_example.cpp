// Example: Integrating render batching in Hyprland's render loop

#include "../src/render/OpenGL.hpp"
#include "../src/render/BatchManager.hpp"
#include "../src/Compositor.hpp"
#include "../src/Window.hpp"

// Example 1: Batching window decorations
void renderWindowDecorationsWithBatching(CHyprOpenGLImpl* opengl) {
    auto* batchManager = opengl->getBatchManager();
    
    // Begin batching for all window decorations
    batchManager->beginBatch();
    
    // Iterate through all windows
    for (auto& window : g_pCompositor->m_vWindows) {
        if (!window->isVisible())
            continue;
            
        // Window borders (same shader state)
        CHyprColor borderColor(0.2f, 0.3f, 0.4f, 1.0f);
        int borderSize = 2;
        int rounding = 10;
        
        CBox windowBox = window->getWindowMainSurfaceBox();
        
        // Add border elements to batch
        // Top border
        batchManager->addRect(CBox(windowBox.x - borderSize, windowBox.y - borderSize, 
                                  windowBox.width + 2 * borderSize, borderSize), 
                             borderColor, rounding, 2.0f);
        // Bottom border
        batchManager->addRect(CBox(windowBox.x - borderSize, windowBox.y + windowBox.height, 
                                  windowBox.width + 2 * borderSize, borderSize), 
                             borderColor, rounding, 2.0f);
        // Left border
        batchManager->addRect(CBox(windowBox.x - borderSize, windowBox.y, 
                                  borderSize, windowBox.height), 
                             borderColor, rounding, 2.0f);
        // Right border
        batchManager->addRect(CBox(windowBox.x + windowBox.width, windowBox.y, 
                                  borderSize, windowBox.height), 
                             borderColor, rounding, 2.0f);
    }
    
    // End batching - all borders rendered with minimal draw calls
    batchManager->endBatch();
}

// Example 2: Batching UI elements
void renderUIElementsWithBatching(CHyprOpenGLImpl* opengl) {
    auto* batchManager = opengl->getBatchManager();
    
    // Different batches for different UI element types
    
    // Batch 1: Status bar background
    batchManager->beginBatch();
    CHyprColor barColor(0.1f, 0.1f, 0.1f, 0.9f);
    batchManager->addRect(CBox(0, 0, 1920, 30), barColor, 0, 2.0f);
    batchManager->endBatch();
    
    // Batch 2: Workspace indicators (all same style)
    batchManager->beginBatch();
    CHyprColor activeColor(0.4f, 0.6f, 0.8f, 1.0f);
    CHyprColor inactiveColor(0.2f, 0.2f, 0.2f, 0.5f);
    
    for (int i = 0; i < 10; i++) {
        bool isActive = (i == g_pCompositor->m_pLastMonitor->activeWorkspaceID());
        CHyprColor color = isActive ? activeColor : inactiveColor;
        
        // Workspace indicator squares
        batchManager->addRect(CBox(50 + i * 35, 5, 30, 20), color, 5, 2.0f);
    }
    batchManager->endBatch();
    
    // Batch 3: Notification backgrounds (if any)
    if (!g_pCompositor->m_dNotifications.empty()) {
        batchManager->beginBatch();
        CHyprColor notifBg(0.15f, 0.15f, 0.15f, 0.95f);
        
        int y = 50;
        for (auto& notif : g_pCompositor->m_dNotifications) {
            batchManager->addRect(CBox(1920 - 350, y, 330, 80), notifBg, 10, 2.0f);
            y += 90;
        }
        batchManager->endBatch();
    }
}

// Example 3: Performance monitoring with batching
void renderPerformanceOverlay(CHyprOpenGLImpl* opengl) {
    auto* batchManager = opengl->getBatchManager();
    auto metrics = batchManager->getMetrics();
    
    // Reset metrics for next frame
    batchManager->resetMetrics();
    
    // Render performance stats
    if (g_pCompositor->m_bShowFPS) {
        // Create text showing batch statistics
        std::string perfText = std::format(
            "Draw Calls: {} | State Changes: {} | Texture Binds: {}",
            metrics.drawCalls, metrics.stateChanges, metrics.textureBinds
        );
        
        // Render background for stats
        batchManager->beginBatch();
        CHyprColor bgColor(0.0f, 0.0f, 0.0f, 0.7f);
        batchManager->addRect(CBox(10, 50, 400, 30), bgColor, 5, 2.0f);
        batchManager->endBatch();
        
        // Render text (not batched)
        auto textTex = opengl->renderText(perfText, CHyprColor(1.0f, 1.0f, 1.0f, 1.0f), 12);
        if (textTex) {
            opengl->renderTexture(textTex, CBox(15, 55, 390, 20), 1.0f);
        }
    }
}

// Example 4: Conditional batching based on scene complexity
void adaptiveBatchingExample(CHyprOpenGLImpl* opengl) {
    auto* batchManager = opengl->getBatchManager();
    
    // Count visible windows
    int visibleWindows = 0;
    for (auto& window : g_pCompositor->m_vWindows) {
        if (window->isVisible())
            visibleWindows++;
    }
    
    // Enable auto-flush for complex scenes
    if (visibleWindows > 20) {
        batchManager->setAutoFlush(true);
        Debug::log(LOG, "Enabled auto-flush for {} visible windows", visibleWindows);
    } else {
        batchManager->setAutoFlush(false);
    }
    
    // Use different batching strategies based on complexity
    if (visibleWindows > 50) {
        // For very complex scenes, batch aggressively
        batchManager->beginBatch();
        
        // Render all window backgrounds first
        for (auto& window : g_pCompositor->m_vWindows) {
            if (!window->isVisible())
                continue;
                
            CHyprColor bgColor(0.1f, 0.1f, 0.1f, 0.8f);
            batchManager->addRect(window->getWindowMainSurfaceBox(), bgColor, 10, 2.0f);
        }
        
        batchManager->endBatch();
        
        // Then render window contents separately
        // ... window content rendering ...
    } else {
        // For simpler scenes, use normal per-window rendering
        // ... standard rendering path ...
    }
}