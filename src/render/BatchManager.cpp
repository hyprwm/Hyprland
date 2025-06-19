#include "BatchManager.hpp"
#include "OpenGL.hpp"
#include "../Compositor.hpp"

CRenderBatchManager::CRenderBatchManager() {
    // OpenGL context will be set when batch manager is used
    m_gl = nullptr;
}

CRenderBatchManager::~CRenderBatchManager() {
    if (m_batching) {
        endBatch();
    }
}

void CRenderBatchManager::beginBatch() {
    if (m_batching) {
        return;
    }
    
    m_batching = true;
    clearBatch();
}

void CRenderBatchManager::endBatch() {
    if (!m_batching) {
        return;
    }
    
    flushBatch();
    m_batching = false;
}

void CRenderBatchManager::addRect(const CBox& box, const CHyprColor& color, int round, float roundingPower) {
    if (!m_batching) {
        // Immediate mode - render directly
        if (m_gl) {
            m_gl->renderRect(box, color, round, roundingPower);
        }
        return;
    }
    
    auto key = createKey(ERenderOperation::RECT, round, roundingPower);
    
    if (m_autoFlush && shouldFlush(key)) {
        flushBatch();
    }
    
    auto& batch = m_batches[key];
    if (!batch) {
        batch = std::make_unique<SRenderBatch>();
        batch->type = ERenderOperation::RECT;
        batch->round = round;
        batch->roundingPower = roundingPower;
        m_batchOrder.push_back(key);
    }
    
    batch->boxes.push_back(box);
    batch->colors.push_back(color);
    m_metrics.pendingOperations++;
}

void CRenderBatchManager::addTexture(uint32_t textureId, const CBox& box, float alpha, int round, float roundingPower) {
    if (!m_batching) {
        // Immediate mode would require actual texture object
        return;
    }
    
    auto key = createKey(ERenderOperation::TEXTURE, round, roundingPower, textureId);
    
    if (m_autoFlush && shouldFlush(key)) {
        flushBatch();
    }
    
    auto& batch = m_batches[key];
    if (!batch) {
        batch = std::make_unique<SRenderBatch>();
        batch->type = ERenderOperation::TEXTURE;
        batch->round = round;
        batch->roundingPower = roundingPower;
        m_batchOrder.push_back(key);
    }
    
    batch->boxes.push_back(box);
    batch->alphas.push_back(alpha);
    batch->textureIds.push_back(textureId);
    m_metrics.pendingOperations++;
}

void CRenderBatchManager::addBorder(const CBox& box, const CHyprColor& color, int round, float roundingPower, int borderSize) {
    if (!m_batching) {
        return;
    }
    
    auto key = createKey(ERenderOperation::BORDER, round, roundingPower);
    
    if (m_autoFlush && shouldFlush(key)) {
        flushBatch();
    }
    
    auto& batch = m_batches[key];
    if (!batch) {
        batch = std::make_unique<SRenderBatch>();
        batch->type = ERenderOperation::BORDER;
        batch->round = round;
        batch->roundingPower = roundingPower;
        m_batchOrder.push_back(key);
    }
    
    batch->boxes.push_back(box);
    batch->colors.push_back(color);
    batch->borderSizes.push_back(borderSize);
    m_metrics.pendingOperations++;
}

void CRenderBatchManager::addShadow(const CBox& box, int round, float roundingPower, int range, const CHyprColor& color) {
    if (!m_batching) {
        return;
    }
    
    auto key = createKey(ERenderOperation::SHADOW, round, roundingPower);
    
    if (m_autoFlush && shouldFlush(key)) {
        flushBatch();
    }
    
    auto& batch = m_batches[key];
    if (!batch) {
        batch = std::make_unique<SRenderBatch>();
        batch->type = ERenderOperation::SHADOW;
        batch->round = round;
        batch->roundingPower = roundingPower;
        m_batchOrder.push_back(key);
    }
    
    batch->boxes.push_back(box);
    batch->colors.push_back(color);
    batch->shadowRanges.push_back(range);
    m_metrics.pendingOperations++;
}

void CRenderBatchManager::flushBatch() {
    if (m_batches.empty()) {
        return;
    }
    
    // Execute batches in order
    for (const auto& key : m_batchOrder) {
        auto it = m_batches.find(key);
        if (it != m_batches.end() && it->second) {
            executeBatch(*it->second);
        }
    }
    
    clearBatch();
}

void CRenderBatchManager::clearBatch() {
    m_batches.clear();
    m_batchOrder.clear();
    m_metrics.pendingOperations = 0;
}

void CRenderBatchManager::resetMetrics() {
    m_metrics = SBatchMetrics{};
}

void CRenderBatchManager::executeBatch(const SRenderBatch& batch) {
    if (batch.boxes.empty() || !m_gl) {
        return;
    }
    
    m_metrics.stateChanges++;
    
    switch (batch.type) {
        case ERenderOperation::RECT:
            if (m_useOptimizedPath && batch.boxes.size() > 3) {
                // Use optimized batch renderer for multiple rects
                m_rectRenderer.beginBatch(batch.round, batch.roundingPower);
                for (size_t i = 0; i < batch.boxes.size(); ++i) {
                    m_rectRenderer.addRect(batch.boxes[i], batch.colors[i]);
                }
                m_rectRenderer.endBatch();
                m_metrics.drawCalls++; // Single draw call for entire batch
            } else {
                // Fall back to individual rendering for small batches
                for (size_t i = 0; i < batch.boxes.size(); ++i) {
                    m_gl->renderRect(batch.boxes[i], batch.colors[i], batch.round, batch.roundingPower);
                }
                m_metrics.drawCalls += batch.boxes.size();
            }
            break;
            
        case ERenderOperation::TEXTURE:
            // Group by texture ID for efficient binding
            if (!batch.textureIds.empty()) {
                m_metrics.textureBinds++;
                // Texture rendering would happen here with proper texture objects
            }
            m_metrics.drawCalls++;
            break;
            
        case ERenderOperation::BORDER:
            // Border rendering would be implemented here
            m_metrics.drawCalls++;
            break;
            
        case ERenderOperation::SHADOW:
            // Shadow rendering would be implemented here  
            m_metrics.drawCalls++;
            break;
    }
}

bool CRenderBatchManager::shouldFlush(const SBatchKey& newKey) const {
    // Check if adding this would exceed reasonable batch size
    const size_t MAX_BATCH_SIZE = 1000;
    
    auto it = m_batches.find(newKey);
    if (it != m_batches.end() && it->second) {
        return it->second->boxes.size() >= MAX_BATCH_SIZE;
    }
    
    return false;
}

CRenderBatchManager::SBatchKey CRenderBatchManager::createKey(ERenderOperation type, int round, float roundingPower, uint32_t textureId) const {
    return SBatchKey{type, round, roundingPower, textureId};
}

void CRenderBatchManager::setOpenGLContext(CHyprOpenGLImpl* gl) { 
    m_gl = gl;
    if (gl) {
        m_rectRenderer.init(gl);
    }
}