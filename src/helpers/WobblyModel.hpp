#pragma once

#include "Vector2D.hpp"
#include <array>

constexpr int GRID_WIDTH = 4;
constexpr int GRID_HEIGHT = 4;

constexpr int MODEL_MAX_SPRINGS = GRID_WIDTH * GRID_HEIGHT * 2;

class CWindow;

class CWobblyModel {
  public:
    CWobblyModel(CWindow* window);
    ~CWobblyModel();

    CWindow* m_pWindow = nullptr;

    int m_iWobblyBits = 1;

    struct Edge {
        float m_fNext;
        float m_fPrev;

        float m_fStart;
        float m_fEnd;

        float m_fAttract;
        float m_fVelocity;
    };

    struct Object {
        Vector2D  m_vForce;
        Vector2D  m_vPosition;
        Vector2D  m_vVelocity;
        float     m_fTheta;
        bool      m_bImmobile;
        Edge      m_sVertEdge;
        Edge      m_sHorzEdge;
    };

    struct Spring {
        Object*   m_pA = nullptr;
        Object*   m_pB = nullptr;
        Vector2D  m_vOffset;
    };

    struct Model {
        Object*                                m_pObjects = nullptr;
        int                                    m_iNumObjects;
        
        std::array<Spring, MODEL_MAX_SPRINGS>  m_aSprings;

        int                                    m_iNumSprings;
        Object*                                m_pAnchorObject = nullptr;
        float                                  m_fSteps;
        Vector2D                               m_vTopLeft;
        Vector2D                               m_vBottomRight;
    };

    Model* m_pModel = nullptr;

    CWindow* m_cWindow = nullptr;
    int m_iXCells = 8;
    int m_iYCells = 8;

    bool m_bGrabbed = false;
    bool m_bSynced  = true;
    int m_iVertexCount = 0;

    // TODO this should change for rendering
    float *m_v  = nullptr;
    float *m_uv = nullptr;

    void notifyGrab(const Vector2D& position);
    void notifyUngrab();
    void notifyMove(const Vector2D& delta);
    void notifyResize(const Vector2D& delta);

    void tick(int deltaTime);
    void calcGeometry();

  private:
    Object* findNearestObject(float x, float y);
    bool ensureHasModel();

    static Model* createModel(int x, int y, int width, int height);
    static void initObjects(Model* model, int x, int y, int width, int height);
    static void initObject(Object* model, int x, int y, int velX, int velY);
    static void initSprings(Model* model, int width, int height);
    static void addSpring(Model* model, Object* a, Object* b, float offsetX, float offsetY);
    static void calcBounds(Model* model);

    // the simulation itself
    static int modelStep(Model* model, float friction, float k, float time);
    static int objectStep(Object* object, float friction, float* force);
    static void springExertForces(Spring* spring, float k);
    static void objectApplyForce(Object* object, float forceX, float forceY);
};
