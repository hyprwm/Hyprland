#pragma once

#include "Vector2D.hpp"
#include <array>
#include <vector>

// TODO where should these go?
constexpr Vector2D GRID_SIZE(4, 4);
constexpr int MODEL_MAX_SPRINGS = GRID_SIZE.x * GRID_SIZE.y * 2;

class CWindow;

class CWobblyModel {
  public:
    CWobblyModel(CWindow* window);
    ~CWobblyModel();

    CWindow* m_pWindow = nullptr;

    int m_iWobblyBits = 1;

    struct Edge {
        float m_fNext{};
        float m_fPrev{};

        float m_fStart{};
        float m_fEnd{};

        float m_fAttract{};
        float m_fVelocity{};
    };

    struct Object {
        Vector2D  m_vForce{};
        Vector2D  m_vPosition{};
        Vector2D  m_vVelocity{};
        float     m_fTheta{};
        bool      m_bImmobile{};
        Edge      m_sVertEdge{};
        Edge      m_sHorzEdge{};
    };

    struct Spring {
        Object*   m_pA = nullptr;
        Object*   m_pB = nullptr;
        Vector2D  m_vOffset{};
    };

    // the model itself
    std::vector<Object>                    m_vObjects;
    int                                    m_iNumObjects;
    
    std::array<Spring, MODEL_MAX_SPRINGS>  m_aSprings;

    int                                    m_iNumSprings;
    Object*                                m_pAnchorObject = nullptr;
    float                                  m_fSteps;
    Vector2D                               m_vTopLeft;
    Vector2D                               m_vBottomRight;


    CWindow* m_cWindow = nullptr;
    int m_iXCells = 8;
    int m_iYCells = 8;

    bool m_bGrabbed = false;
    bool m_bSynced  = true;
    int m_iVertexCount = 0;

    // TODO this should change for rendering
    std::vector<float> m_vVerts;
    std::vector<float> m_vUVs;

    void notifyGrab(const Vector2D& position);
    void notifyUngrab();
    void notifyMove(const Vector2D& delta);
    void notifyResize(const Vector2D& delta);

    void tick(int deltaTime);
    void calcGeometry();

  private:
    Object* findNearestObject(const Vector2D& position);

    void initObjects(const Vector2D& position, const Vector2D& size);
    void initObject(Object* model, const Vector2D& position, const Vector2D& velocity);
    void initSprings(const Vector2D& size);
    void addSpring(Object* a, Object* b, const Vector2D& offset);
    void calcBounds();

    // simulation itself
    int modelStep(float friction, float k, float time);

    // returns object velocity sum
    int objectStep(Object* object, float friction, float& force);
    void springExertForces(Spring* spring, float k);
    void objectApplyForce(Object* object, const Vector2D& force);
};
