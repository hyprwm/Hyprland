#include "WobblyModel.hpp"

#include "../debug/Log.hpp"
#include "../Window.hpp"
#include "../managers/AnimationManager.hpp"
#include <cmath>
#include <limits>

// TODO what are these? Shouldn't be macros...
#define WobblyInitial  (1L << 0)
#define WobblyForce    (1L << 1)
#define WobblyVelocity (1L << 2)

#define MASS 50.0f
#define WOBBLY_FRICTION 3
#define WOBBLY_SPRING_K 8

CWobblyModel::CWobblyModel(CWindow* window) {
    Debug::log(LOG, "Creating WobblyModel for window %s", window->m_szTitle);

    m_pWindow = window;

    m_iNumObjects = GRID_SIZE.x * GRID_SIZE.y;
    m_vObjects = std::vector<Object>(m_iNumObjects);

    m_pAnchorObject = nullptr;
    m_iNumSprings = 0;

    m_fSteps = 0;

    initObjects(m_pWindow->m_vPosition, m_pWindow->m_vSize);
    initSprings(m_pWindow->m_vSize);

    calcBounds();

    // register
    g_pAnimationManager->m_lWobblyModels.push_back(this);
}

CWobblyModel::~CWobblyModel() {
    Debug::log(LOG, "Destroying WobblyModel for window %s", m_pWindow->m_szTitle);
    // unregister
    g_pAnimationManager->m_lWobblyModels.remove(this);
}

void CWobblyModel::notifyGrab(const Vector2D& position) {
    Debug::log(LOG, "notifyGrab: %f, %f", position.x, position.y);


    if (m_pAnchorObject) {
        m_pAnchorObject->m_bImmobile = false;
    }

    m_pAnchorObject = findNearestObject(position);
    m_pAnchorObject->m_bImmobile = true;

    m_bGrabbed = true;

    for (int i = 0; i < m_iNumSprings; i++) {
        Spring& spring = m_aSprings[i];

        if (spring.m_pA == m_pAnchorObject) {
            spring.m_pB->m_vVelocity = spring.m_pB->m_vVelocity - spring.m_vOffset * 0.05f;
        } else if (spring.m_pB == m_pAnchorObject) {
            spring.m_pA->m_vVelocity = spring.m_pA->m_vVelocity + spring.m_vOffset * 0.05f;
        }
    }

    m_iWobblyBits |= WobblyInitial; // TODO what is this
}

void CWobblyModel::notifyUngrab() {
    Debug::log(LOG, "notifyUngrab");

    if (m_bGrabbed) {
        if (m_pAnchorObject != nullptr) {
            m_pAnchorObject->m_bImmobile = false;
        }

        m_pAnchorObject = nullptr;

        m_iWobblyBits |= WobblyInitial;

        m_bGrabbed = false;
    }
}

void CWobblyModel::notifyMove(const Vector2D &delta) {
    Debug::log(LOG, "notifyMove: %f, %f", delta.x, delta.y);

    if (m_bGrabbed) {
        m_pAnchorObject->m_vPosition += delta;

        m_iWobblyBits |= WobblyInitial;
        m_bSynced = false;
    }
}

void CWobblyModel::notifyResize(const Vector2D &delta) {
    Debug::log(LOG, "notifyResize: %f, %f", delta.x, delta.y);

    // TODO handle this, meh first get the simulation rendering
}

void CWobblyModel::tick(int deltaTime) {
    Debug::log(LOG, "tick: %i", deltaTime);

    float friction = WOBBLY_FRICTION;
    float springK  = WOBBLY_SPRING_K;

    // TODO can't I remove this one?
    if (m_iWobblyBits == 0)
        return;

    if ((m_iWobblyBits & (WobblyInitial | WobblyVelocity | WobblyForce)) == 0)
        return;

    m_iWobblyBits = modelStep(friction, springK, (m_iWobblyBits & WobblyVelocity) ? deltaTime : 16);

    if (m_iWobblyBits != 0) {
        calcBounds();
    } else {
        m_pWindow->m_vPosition.x = m_vTopLeft.x;
        m_pWindow->m_vPosition.y = m_vTopLeft.y;

        m_bSynced = true;
    }
}

void CWobblyModel::calcGeometry() {
    if (m_iWobblyBits == 0)
        return;

    float width  = m_pWindow->m_vSize.x;
    float height = m_pWindow->m_vSize.y;

    float cell_w = width  / m_iXCells;
    float cell_h = height / m_iYCells;

    int iw = m_iXCells + 1;
    int ih = m_iYCells + 1;

    // TODO this is real bad. goes in render function... maybe even shader?
    m_vVerts.reserve(2 * iw * ih);
    m_vUVs.reserve(2 * iw * ih);

    for (int y = 0; y < ih; y++) {
        for (int x = 0; x < iw; x++) {
            float u = (x * cell_w) / width;
            float v = (y * cell_h) / height;

            // evaluate bezier patch
            float coeffsU[4] {
                (1 - u) * (1 - u) * (1 - u),
                3 * u * (1 - u) * (1 - u),
                3 * u * u * (1 - u),
                u * u * u
            };

            float coeffsV[4]{
                (1 - v) * (1 - v) * (1 - v),
                3 * v * (1 - v) * (1 - v),
                3 * v * v * (1 - v),
                v * v * v
            };

            float deformedX = 0.f, deformedY = 0.f;
            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 4; j++) {
                    deformedX += coeffsU[i] * coeffsV[j] * m_vObjects[j * int(GRID_SIZE.x) + i].m_vPosition.x;
                    deformedY += coeffsU[i] * coeffsV[j] * m_vObjects[j * int(GRID_SIZE.y) + i].m_vPosition.y;
                }
            }

            m_vVerts[(y * iw + x) * 2] = deformedX;
            m_vVerts[(y * iw + x) * 2 + 1] = deformedY;

            m_vUVs[(y * iw + x) * 2] = (x * cell_w) / width;
            m_vUVs[(y * iw + x) * 2 + 1] = (y * cell_h) / height;
        }
    }
}


CWobblyModel::Object* CWobblyModel::findNearestObject(const Vector2D& position) {
    Object* nearest = &m_vObjects[0];

    // start infinitely far away, so any object is closer
    float nearestDistSq = std::numeric_limits<float>::max();

    for (int i = 0; i < m_iNumObjects; i++) {
        Object* curObj = &m_vObjects[i];

        Vector2D delta = curObj->m_vPosition - position;
        float distSq = delta.x * delta.x + delta.y * delta.y; // no need for sqrt
        
        if (distSq < nearestDistSq) {
            nearestDistSq = distSq;
            nearest = curObj;
        }
    }

    return nearest;
}

void CWobblyModel::initObjects(const Vector2D& position, const Vector2D& size) {
    int i = 0;

    Vector2D gridDiv = GRID_SIZE - 1;

    for (int gridY = 0; gridY < GRID_SIZE.y; gridY++) {
        for (int gridX = 0; gridX < GRID_SIZE.x; gridX++) {
            
            // init object
            m_vObjects[i] = Object {
                Vector2D(),
                position + (Vector2D(gridX, gridY) * size) / gridDiv
            };
        }
    }
}

void CWobblyModel::initSprings(const Vector2D& size) {
    Vector2D padding = size / (GRID_SIZE  - 1);

    m_iNumSprings = 0;

    int i = 0;

    // TODO can't I just start at 1?
    for (int gridY = 0; gridY < GRID_SIZE.y; gridY++) {
        for (int gridX = 0; gridX < GRID_SIZE.x; gridX++) {
            if (gridX > 0) {
                addSpring(&m_vObjects[i - 1], &m_vObjects[i], Vector2D(padding.x, 0));
            }

            if (gridY > 0) {
                addSpring(&m_vObjects[i - int(GRID_SIZE.x)], &m_vObjects[i], Vector2D(0, padding.y));
            }

            i++;
        }
    }
}

void CWobblyModel::addSpring(Object* a, Object* b, const Vector2D& offset) {
    m_aSprings[m_iNumSprings] = Spring{
        a, b, offset
    };
    m_iNumSprings++;
}

void CWobblyModel::calcBounds() {
    m_vTopLeft.x = std::numeric_limits<short>::max();
    m_vTopLeft.y = std::numeric_limits<short>::max();
    m_vBottomRight.x = std::numeric_limits<short>::min();
    m_vBottomRight.y = std::numeric_limits<short>::min();

    for (int i = 0; i < m_iNumObjects; i++) {
        if (m_vObjects[i].m_vPosition.x < m_vTopLeft.x) {
            m_vTopLeft.x = m_vObjects[i].m_vPosition.x;
        } else if (m_vObjects[i].m_vPosition.x > m_vBottomRight.x) {
            m_vBottomRight.x = m_vObjects[i].m_vPosition.x;
        }
    
        if (m_vObjects[i].m_vPosition.y < m_vTopLeft.y) {
            m_vTopLeft.y = m_vObjects[i].m_vPosition.y;
        } else if (m_vObjects[i].m_vPosition.y > m_vBottomRight.y) {
            m_vBottomRight.y = m_vObjects[i].m_vPosition.y;
        }
    }
}

int CWobblyModel::modelStep(float friction, float k, float time) {
    int   wobblyBits = 0;
    float velocitySum = 0.0f;
    float force, forceSum = 0.0f;

    m_fSteps += time / 15.0f;
    int steps = floor (m_fSteps);
    m_fSteps -= steps;

    if (!steps)
        return 1;

    for (int j = 0; j < steps; j++) {
        for (int i = 0; i < m_iNumSprings; i++) {
            springExertForces(&m_aSprings[i], k);
        }
    
        for (int i = 0; i < m_iNumObjects; i++) {
            velocitySum += objectStep(&m_vObjects[i],
                            friction,
                            force);
            forceSum += force;
        }
    }

    calcBounds();

    if (velocitySum > 0.5f)
        wobblyBits |= WobblyVelocity;

    if (forceSum > 20.0f)
        wobblyBits |= WobblyForce;

    return wobblyBits;
}

int CWobblyModel::objectStep(Object* object, float friction, float& force) {
    object->m_fTheta += 0.05f;

    if (object->m_bImmobile) {
        object->m_vVelocity = Vector2D();
        object->m_vForce = Vector2D();

        force = 0;
        return 0;
    } else {
        object->m_vForce -= object->m_vVelocity * friction;
        object->m_vVelocity += object->m_vForce / MASS;
        object->m_vPosition += object->m_vVelocity;

        force = std::abs(object->m_vForce.x) + std::abs(object->m_vForce.y);
        object->m_vForce = Vector2D();

        return std::abs(object->m_vVelocity.x) + std::abs(object->m_vVelocity.y);
    }
}

void CWobblyModel::springExertForces(Spring* spring, float k) {
    Vector2D a = spring->m_pA->m_vPosition;
    Vector2D b = spring->m_pB->m_vPosition;

    Vector2D deltaA = (b - a - spring->m_vOffset) * 0.5f;
    Vector2D deltaB = (a - b - spring->m_vOffset) * 0.5f;

    objectApplyForce(spring->m_pA, deltaA * k);
    objectApplyForce(spring->m_pB, deltaB * k);
}

void CWobblyModel::objectApplyForce(Object* object, const Vector2D& force) {
    object->m_vForce += force;
}
