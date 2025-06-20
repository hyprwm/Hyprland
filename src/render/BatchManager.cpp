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

bool CRenderBatchManager::beginBatch() {
    if (m_batching) {
        return true; // Already batching
    }
    
    if (!m_gl) {
        Debug::log(ERR, "BatchManager: Cannot begin batch without OpenGL context");
        return false;
    }

    m_batching = true;
    clearBatch();
    return true;
}

bool CRenderBatchManager::endBatch() {
    if (!m_batching) {
        return false;
    }

    bool result = flushBatch();
    m_batching = false;
    return result;
}

bool CRenderBatchManager::addRect(const CBox& box, const CHyprColor& color, int round, float roundingPower) {
    if (!m_batching) {
        // Immediate mode - render directly
        if (!m_gl) {
            Debug::log(ERR, "BatchManager: Cannot render rect without OpenGL context");
            return false;
        }
        m_gl->renderRect(box, color, round, roundingPower);
        return true;
    }

    auto key = createKey(ERenderOperation::RECT, round, roundingPower);

    if (m_autoFlush && shouldFlush(key)) {
        flushBatch();
    }

    auto& batch = m_batches[key];
    if (!batch) {
        batch                = std::make_unique<SRenderBatch>();
        batch->type          = ERenderOperation::RECT;
        batch->round         = round;
        batch->roundingPower = roundingPower;
        m_batchOrder.push_back(key);
    }

    // Check memory limits
    if (m_metrics.pendingOperations >= MAX_INSTANCES_PER_DRAW * 10) {
        Debug::log(WARN, "BatchManager: Too many pending operations, flushing early");
        flushBatch();
    }
    
    batch->boxes.push_back(box);
    batch->colors.push_back(color);
    m_metrics.pendingOperations++;
    return true;
}

bool CRenderBatchManager::addTexture(uint32_t textureId, const CBox& box, float alpha, int round, float roundingPower) {
    if (!m_batching) {
        // Immediate mode would require actual texture object
        Debug::log(WARN, "BatchManager: Cannot add texture in immediate mode");
        return false;
    }

    auto key = createKey(ERenderOperation::TEXTURE, round, roundingPower, textureId);

    if (m_autoFlush && shouldFlush(key)) {
        flushBatch();
    }

    auto& batch = m_batches[key];
    if (!batch) {
        batch                = std::make_unique<SRenderBatch>();
        batch->type          = ERenderOperation::TEXTURE;
        batch->round         = round;
        batch->roundingPower = roundingPower;
        m_batchOrder.push_back(key);
    }

    // Check memory limits
    if (m_metrics.pendingOperations >= MAX_INSTANCES_PER_DRAW * 10) {
        Debug::log(WARN, "BatchManager: Too many pending operations, flushing early");
        flushBatch();
    }
    
    batch->boxes.push_back(box);
    batch->alphas.push_back(alpha);
    batch->textureIds.push_back(textureId);
    m_metrics.pendingOperations++;
    return true;
}

bool CRenderBatchManager::addBorder(const CBox& box, const CHyprColor& color, int round, float roundingPower, int borderSize) {
    if (!m_batching) {
        return false;
    }

    auto key = createKey(ERenderOperation::BORDER, round, roundingPower);

    if (m_autoFlush && shouldFlush(key)) {
        flushBatch();
    }

    auto& batch = m_batches[key];
    if (!batch) {
        batch                = std::make_unique<SRenderBatch>();
        batch->type          = ERenderOperation::BORDER;
        batch->round         = round;
        batch->roundingPower = roundingPower;
        m_batchOrder.push_back(key);
    }

    // Check memory limits
    if (m_metrics.pendingOperations >= MAX_INSTANCES_PER_DRAW * 10) {
        Debug::log(WARN, "BatchManager: Too many pending operations, flushing early");
        flushBatch();
    }
    
    batch->boxes.push_back(box);
    batch->colors.push_back(color);
    batch->borderSizes.push_back(borderSize);
    m_metrics.pendingOperations++;
    return true;
}

bool CRenderBatchManager::addShadow(const CBox& box, int round, float roundingPower, int range, const CHyprColor& color) {
    if (!m_batching) {
        // Immediate mode - render directly
        if (m_gl) {
            m_gl->renderRoundedShadow(box, round, roundingPower, range, color, 1.0f);
        }
        return false;
    }

    auto key = createKey(ERenderOperation::SHADOW, round, roundingPower);

    if (m_autoFlush && shouldFlush(key)) {
        flushBatch();
    }

    auto& batch = m_batches[key];
    if (!batch) {
        batch                = std::make_unique<SRenderBatch>();
        batch->type          = ERenderOperation::SHADOW;
        batch->round         = round;
        batch->roundingPower = roundingPower;
        m_batchOrder.push_back(key);
    }

    batch->boxes.push_back(box);
    batch->colors.push_back(color);
    batch->shadowRanges.push_back(range);
    m_metrics.pendingOperations++;
    return true;
}

bool CRenderBatchManager::flushBatch() {
    if (m_batches.empty()) {
        return false;
    }

    size_t totalOperations = 0;
    size_t batchCount      = 0;

    // Execute batches in order
    for (const auto& key : m_batchOrder) {
        auto it = m_batches.find(key);
        if (it != m_batches.end() && it->second) {
            totalOperations += it->second->boxes.size();
            batchCount++;
            executeBatch(*it->second);
            m_metrics.drawCalls++; // One draw call per batch
        }
    }

#ifdef HYPRLAND_DEBUG
    // Update efficiency metrics (only in debug builds)
    if (totalOperations > batchCount) {
        static size_t totalOpsEver     = 0;
        static size_t totalBatchesEver = 0;
        totalOpsEver += totalOperations;
        totalBatchesEver += batchCount;

        // Log efficiency occasionally for debugging
        static int logCounter = 0;
        if (++logCounter % 100 == 0) {
            float efficiency = 100.0f * (1.0f - (float)totalBatchesEver / totalOpsEver);
            Debug::log(LOG, "Batching efficiency: {:.1f}% ({} ops in {} batches)", efficiency, totalOpsEver, totalBatchesEver);
        }
    }
#endif

    clearBatch();
    return true;
}

void CRenderBatchManager::clearBatch() {
    m_batches.clear();
    m_batchOrder.clear();
    m_metrics.pendingOperations = 0;
}

void CRenderBatchManager::resetMetrics() {
    m_metrics = SBatchMetrics{};
}

size_t CRenderBatchManager::getPendingOperations() const {
    size_t total = 0;
    for (const auto& [key, batch] : m_batches) {
        if (batch) {
            total += batch->boxes.size();
        }
    }
    return total;
}

bool CRenderBatchManager::testBatchingEfficiency() {
    if (!m_batching)
        return false;

    // Test scenario: Add many similar rectangles
    const int        TEST_RECTS = 50;
    const CHyprColor testColor(1.0f, 0.0f, 0.0f, 1.0f);

    resetMetrics();
    beginBatch();

    // Add many similar rectangles (should batch well)
    for (int i = 0; i < TEST_RECTS; i++) {
        CBox box(i * 10, i * 10, 50, 50);
        addRect(box, testColor, 5, 2.0f); // Same round and power = good batching
    }

    size_t batchCount = getBatchCount();
    size_t operations = getPendingOperations();

    endBatch();

    // Efficiency test: we should have far fewer batches than operations
    bool efficient = (batchCount > 0) && (operations > batchCount * 3);

    return efficient;
}

void CRenderBatchManager::printPerformanceReport() const {
    Debug::log(LOG, "=== Render Batch Manager Performance Report ===");
    Debug::log(LOG, "Total draw calls: {}", m_metrics.drawCalls);
    Debug::log(LOG, "- Instanced draw calls: {} ({:.1f}%)", m_metrics.instancedDrawCalls,
               m_metrics.drawCalls > 0 ? (100.0f * m_metrics.instancedDrawCalls / m_metrics.drawCalls) : 0);
    Debug::log(LOG, "- Batched draw calls: {} ({:.1f}%)", m_metrics.batchedDrawCalls, m_metrics.drawCalls > 0 ? (100.0f * m_metrics.batchedDrawCalls / m_metrics.drawCalls) : 0);
    Debug::log(LOG, "State changes: {}", m_metrics.stateChanges);
    Debug::log(LOG, "Texture binds: {}", m_metrics.textureBinds);
    Debug::log(LOG, "Instances rendered: {}", m_metrics.instancesRendered);
    Debug::log(LOG, "Vertices rendered: {}", m_metrics.verticesRendered);

    float efficiency = getInstancedRenderingEfficiency();
    Debug::log(LOG, "Instanced rendering efficiency: {:.1f}%", efficiency * 100.0f);
    Debug::log(LOG, "==============================================");
}

float CRenderBatchManager::getInstancedRenderingEfficiency() const {
    if (m_metrics.instancedDrawCalls == 0) {
        return 0.0f;
    }

    // Calculate efficiency based on instances rendered vs draw calls
    // Perfect efficiency would be rendering all instances in one draw call
    float avgInstancesPerDraw = (float)m_metrics.instancesRendered / m_metrics.instancedDrawCalls;

    // Normalize to 0-1 range (assuming 100+ instances per draw is excellent)
    return std::min(1.0f, avgInstancesPerDraw / 100.0f);
}

void CRenderBatchManager::executeBatch(const SRenderBatch& batch) {
    if (batch.boxes.empty() || !m_gl) {
        return;
    }

    m_metrics.stateChanges++;

    switch (batch.type) {
        case ERenderOperation::RECT:
            if (m_useOptimizedPath && batch.boxes.size() > 3) {
                // Decide between instanced and batched rendering
                if (getUseInstancing() && batch.boxes.size() > 20) {
                    // Use instanced rendering for large batches
                    m_instancedRenderer.beginBatch(batch.round, batch.roundingPower);
                    for (size_t i = 0; i < batch.boxes.size(); ++i) {
                        m_instancedRenderer.addRect(batch.boxes[i], batch.colors[i]);
                    }
                    m_instancedRenderer.endBatch();
                    m_metrics.drawCalls++;
                    m_metrics.instancedDrawCalls++;
                    m_metrics.instancesRendered += batch.boxes.size();
                } else {
                    // Use optimized batch renderer for medium batches
                    m_rectRenderer.beginBatch(batch.round, batch.roundingPower);
                    for (size_t i = 0; i < batch.boxes.size(); ++i) {
                        m_rectRenderer.addRect(batch.boxes[i], batch.colors[i]);
                    }
                    m_rectRenderer.endBatch();
                    m_metrics.drawCalls++;
                    m_metrics.batchedDrawCalls++;
                    m_metrics.verticesRendered += batch.boxes.size() * 4; // 4 vertices per rect
                }
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
            // Render all shadows in the batch
            for (size_t i = 0; i < batch.boxes.size(); ++i) {
                m_gl->renderRoundedShadow(batch.boxes[i], batch.round, batch.roundingPower, batch.shadowRanges[i], batch.colors[i], 1.0f);
            }
            m_metrics.drawCalls++;
            break;
    }
}

bool CRenderBatchManager::shouldFlush(const SBatchKey& newKey) const {
    // Check if adding this would exceed reasonable batch size
    const size_t MAX_BATCH_SIZE = 1000;

    auto         it = m_batches.find(newKey);
    if (it != m_batches.end() && it->second) {
        return it->second->boxes.size() >= MAX_BATCH_SIZE;
    }

    return false;
}

CRenderBatchManager::SBatchKey CRenderBatchManager::createKey(ERenderOperation type, int round, float roundingPower, uint32_t textureId) const {
    return SBatchKey{type, round, roundingPower, textureId};
}