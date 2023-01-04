#pragma once

#include "Vector2D.hpp"

constexpr int GRID_WIDTH = 4;
constexpr int GRID_HEIGHT = 4;

constexpr int MODEL_MAX_SPRINGS = GRID_WIDTH * GRID_HEIGHT * 2;

class CWindow;

class CWobblyModel {
  public:
    CWobblyModel(CWindow* window);

    CWindow* m_pWindow;

    bool m_wobbly;

    struct Edge {
        float next, prev;

        float start;
        float end;

        float attract;
        float velocity;
    };

    struct Object {
        Vector2D  force;
        Vector2D  position;
        Vector2D  velocity;
        float     theta;
        int       immobile;
        Edge      vertEdge;
        Edge      horzEdge;
    };

    struct {
        Object      *objects;
        int         numObjects;
        
        struct {
            Object    *a;
            Object    *b;
            Vector2D  offset;
        } springs[MODEL_MAX_SPRINGS];

        int          numSprings;
        Object       *anchorObject;
        float        steps;
        Vector2D     topLeft;
        Vector2D     bottomRight;
    } m_sModel;

    CWindow* m_cWindow = nullptr;
    int m_iXCells = 8;
    int m_iYCells = 8;

    bool m_bGrabbed = false;
    bool m_bSynced  = true;
    int m_iVertexCount;

    float *v = nullptr; // TODO this was glfloat

    void notifyGrab(const Vector2D& position);
    void notifyUngrab();
    void notifyMove(const Vector2D& delta);
    void notifyResize(const Vector2D& delta);
};
