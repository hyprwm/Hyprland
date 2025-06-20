#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include "../helpers/Color.hpp"
#include "../helpers/math/Math.hpp"
#include "BatchedRectRenderer.hpp"
#include "InstancedRectRenderer.hpp"

class CHyprOpenGLImpl;

enum class ERenderOperation : uint8_t {
    RECT,
    TEXTURE,
    BORDER,
    SHADOW
};

struct SRenderBatch {
    ERenderOperation        type;
    int                     round;
    float                   roundingPower;
    std::vector<CBox>       boxes;
    std::vector<CHyprColor> colors;
    std::vector<float>      alphas;
    std::vector<uint32_t>   textureIds;
    std::vector<int>        borderSizes;
    std::vector<int>        shadowRanges;
};

struct SBatchMetrics {
    size_t drawCalls          = 0;
    size_t stateChanges       = 0;
    size_t textureBinds       = 0;
    size_t pendingOperations  = 0;
    size_t instancedDrawCalls = 0;
    size_t batchedDrawCalls   = 0;
    size_t instancesRendered  = 0;
    size_t verticesRendered   = 0;
};

class CRenderBatchManager {
  public:
    CRenderBatchManager();
    ~CRenderBatchManager();

    void beginBatch();
    void endBatch();
    bool isBatching() const {
        return m_batching;
    }

    void addRect(const CBox& box, const CHyprColor& color, int round, float roundingPower);
    void addTexture(uint32_t textureId, const CBox& box, float alpha, int round, float roundingPower);
    void addBorder(const CBox& box, const CHyprColor& color, int round, float roundingPower, int borderSize);
    void addShadow(const CBox& box, int round, float roundingPower, int range, const CHyprColor& color);

    void flushBatch();
    void clearBatch();

    void setAutoFlush(bool autoFlush) {
        m_autoFlush = autoFlush;
    }
    bool getAutoFlush() const {
        return m_autoFlush;
    }

    SBatchMetrics getMetrics() const {
        return m_metrics;
    }
    void resetMetrics();

    void setOpenGLContext(CHyprOpenGLImpl* gl) {
        m_gl = gl;
        if (gl) {
            m_rectRenderer.init(gl);
            m_instancedRenderer.init(gl);
        }
    }

    // Control whether to use instanced rendering
    void setUseInstancing(bool useInstancing) {
        m_useInstancing = useInstancing;
    }
    bool getUseInstancing() const {
        return m_useInstancing && m_instancedRenderer.isInstancedRenderingSupported();
    }

    // Test methods for performance verification
    size_t getBatchCount() const {
        return m_batches.size();
    }
    size_t getPendingOperations() const;
    bool   testBatchingEfficiency(); // Returns true if batching provides benefits

    // Performance comparison methods
    void  printPerformanceReport() const;
    float getInstancedRenderingEfficiency() const;

  private:
    struct SBatchKey {
        ERenderOperation type;
        int              round;
        float            roundingPower;
        uint32_t         textureId; // Only used for texture operations

        bool             operator==(const SBatchKey& other) const {
            return type == other.type && round == other.round && roundingPower == other.roundingPower && textureId == other.textureId;
        }
    };

    struct SBatchKeyHash {
        std::size_t operator()(const SBatchKey& key) const {
            std::size_t h1 = std::hash<int>{}(static_cast<int>(key.type));
            std::size_t h2 = std::hash<int>{}(key.round);
            std::size_t h3 = std::hash<float>{}(key.roundingPower);
            std::size_t h4 = std::hash<uint32_t>{}(key.textureId);
            return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
        }
    };

    bool                                                                        m_batching  = false;
    bool                                                                        m_autoFlush = false;

    std::unordered_map<SBatchKey, std::unique_ptr<SRenderBatch>, SBatchKeyHash> m_batches;
    std::vector<SBatchKey>                                                      m_batchOrder; // Maintain insertion order

    SBatchMetrics                                                               m_metrics;
    CHyprOpenGLImpl*                                                            m_gl = nullptr;
    CBatchedRectRenderer                                                        m_rectRenderer;
    CInstancedRectRenderer                                                      m_instancedRenderer;
    bool                                                                        m_useOptimizedPath = true;
    bool                                                                        m_useInstancing    = true; // Default to using instancing if available

    void                                                                        executeBatch(const SRenderBatch& batch);
    bool                                                                        shouldFlush(const SBatchKey& newKey) const;
    SBatchKey                                                                   createKey(ERenderOperation type, int round, float roundingPower, uint32_t textureId = 0) const;
};