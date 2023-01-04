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
    m_pWindow = window;

    ensureHasModel();

    // register
    g_pAnimationManager->m_lWobblyModels.push_back(this);
}

CWobblyModel::~CWobblyModel() {
    // unregister
    g_pAnimationManager->m_lWobblyModels.remove(this);

    if (m_pModel != nullptr) {
        delete[] m_pModel->m_pObjects;
        delete m_pModel;
    }

    // TODO probs remove this when rendering is figured out
    if(m_v)
        delete[] m_v;
    if(m_uv)
        delete[] m_uv;
}

void CWobblyModel::notifyGrab(const Vector2D& position) {
    Debug::log(LOG, "notifyGrab: %f, %f", position.x, position.y);


    if (ensureHasModel())
    {
        if (m_pModel->m_pAnchorObject) {
            m_pModel->m_pAnchorObject->m_bImmobile = false;
        }

        m_pModel->m_pAnchorObject = findNearestObject(position.x, position.y);
        m_pModel->m_pAnchorObject->m_bImmobile = true;

        m_bGrabbed = true;

        for (int i = 0; i < m_pModel->m_iNumSprings; i++) {
            Spring& spring = m_pModel->m_aSprings[i];

            if (spring.m_pA == m_pModel->m_pAnchorObject) {
                spring.m_pB->m_vVelocity.x -= spring.m_vOffset.x * 0.05f;
                spring.m_pB->m_vVelocity.y -= spring.m_vOffset.y * 0.05f;
            } else if (spring.m_pB == m_pModel->m_pAnchorObject) {
                spring.m_pA->m_vVelocity.x += spring.m_vOffset.x * 0.05f;
                spring.m_pA->m_vVelocity.y += spring.m_vOffset.y * 0.05f;
            }
        }

        m_iWobblyBits |= WobblyInitial; // TODO what is this
    }
}

void CWobblyModel::notifyUngrab() {
    Debug::log(LOG, "notifyUngrab");

    if (m_bGrabbed) {
        if (m_pModel != nullptr) {
            if (m_pModel->m_pAnchorObject != nullptr) {
                m_pModel->m_pAnchorObject->m_bImmobile = false;
            }

            m_pModel->m_pAnchorObject = nullptr;

            m_iWobblyBits |= WobblyInitial;
        }

        m_bGrabbed = false;
    }
}

void CWobblyModel::notifyMove(const Vector2D &delta) {
    Debug::log(LOG, "notifyMove: %f, %f", delta.x, delta.y);

    if (m_bGrabbed) {
        m_pModel->m_pAnchorObject->m_vPosition.x += delta.x;
        m_pModel->m_pAnchorObject->m_vPosition.y += delta.y;

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

    if(m_pModel == nullptr)
        return;
    
    float friction = WOBBLY_FRICTION;
    float springK  = WOBBLY_SPRING_K;

    // TODO can't I remove this one?
    if (m_iWobblyBits == 0)
        return;

    if ((m_iWobblyBits & (WobblyInitial | WobblyVelocity | WobblyForce)) == 0)
        return;

    m_iWobblyBits = modelStep(m_pModel, friction, springK, (m_iWobblyBits & WobblyVelocity) ? deltaTime : 16);

    if (m_iWobblyBits != 0) {
        calcBounds(m_pModel);
    } else {
        m_pWindow->m_vPosition.x = m_pModel->m_vTopLeft.x;
        m_pWindow->m_vPosition.y = m_pModel->m_vTopLeft.y;

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
    delete[] m_v;
    delete[] m_uv;
    m_v = new float[2 * iw * ih]{};
    m_uv = new float[2 * iw * ih]{};

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
                    deformedX += coeffsU[i] * coeffsV[j] * m_pModel->m_pObjects[j * GRID_WIDTH + i].m_vPosition.x;
                    deformedY += coeffsU[i] * coeffsV[j] * m_pModel->m_pObjects[j * GRID_HEIGHT + i].m_vPosition.y;
                }
            }

            m_v[(y * iw + x) * 2] = deformedX;
            m_v[(y * iw + x) * 2 + 1] = deformedY;

            m_uv[(y * iw + x) * 2] = (x * cell_w) / width;
            m_uv[(y * iw + x) * 2 + 1] = (y * cell_h) / height;
        }
    }
}


CWobblyModel::Object* CWobblyModel::findNearestObject(float x, float y) {
    Object* nearest = &m_pModel->m_pObjects[0];

    // start infinitely far away, so any object is closer
    float nearestDistSq = std::numeric_limits<float>::max();

    for (int i = 0; i < m_pModel->m_iNumObjects; i++) {
        Object* curObj = &m_pModel->m_pObjects[i];

        float dx = curObj->m_vPosition.x - x;
        float dy = curObj->m_vPosition.y - y;
        float distSq = dx * dx + dy * dy; // no need for sqrt
        
        if (distSq < nearestDistSq) {
            nearestDistSq = distSq;
            nearest = curObj;
        }
    }

    return nearest;
}

bool CWobblyModel::ensureHasModel() {
    if (!m_pModel) {
        m_pModel = createModel(m_pWindow->m_vPosition.x, m_pWindow->m_vPosition.y, m_pWindow->m_vSize.x, m_pWindow->m_vSize.y);
        if(!m_pModel)
            return false;
    }

    return true; 
}

CWobblyModel::Model* CWobblyModel::createModel(int x, int y, int width, int height) {
    Model* model = new Model{};
    if(!model)
        return nullptr;

    model->m_iNumObjects = GRID_WIDTH * GRID_HEIGHT;
    model->m_pObjects = new Object[model->m_iNumObjects]{};
    if(!model->m_pObjects) {
        delete model;
        return nullptr;
    }

    model->m_pAnchorObject = nullptr;
    model->m_iNumSprings = 0;

    model->m_fSteps = 0;

    initObjects(model, x, y, width, height);
    initSprings(model, width, height);

    calcBounds(model);

    return model;
}

void CWobblyModel::initObjects(Model* model, int x, int y, int width, int height) {
    int i = 0;

    float gw = GRID_WIDTH - 1;
    float gh = GRID_HEIGHT - 1;

    for (int gridY = 0; gridY < GRID_HEIGHT; gridY++) {
        for (int gridX = 0; gridX < GRID_WIDTH; gridX++) {
            
            // init object
            Object& obj = model->m_pObjects[i];
            obj.m_vForce.x = 0;
            obj.m_vForce.y = 0;

            obj.m_vPosition.x = x + (gridX * width ) / gw;
            obj.m_vPosition.y = y + (gridY * height) / gh;

            obj.m_vVelocity.x = 0;
            obj.m_vVelocity.y = 0;

            obj.m_fTheta = 0;
            obj.m_bImmobile = false;

            obj.m_sVertEdge.m_fNext = 0.f;
            obj.m_sHorzEdge.m_fNext = 0.f;
        }
    }
}

void CWobblyModel::initSprings(Model* model, int width, int height) {
    float hpad = float(width ) / (GRID_WIDTH  - 1);
    float vpad = float(height) / (GRID_HEIGHT - 1);

    model->m_iNumSprings = 0;

    int i = 0;

    // TODO can't I just start at 1?
    for (int gridY = 0; gridY < GRID_HEIGHT; gridY++) {
        for (int gridX = 0; gridX < GRID_WIDTH; gridX++) {
            if (gridX > 0) {
                addSpring(model, &model->m_pObjects[i - 1], &model->m_pObjects[i], hpad, 0);
            }

            if (gridY > 0) {
                addSpring(model, &model->m_pObjects[i - GRID_WIDTH], &model->m_pObjects[i], 0, vpad);
            }

            i++;
        }
    }
}

void CWobblyModel::addSpring(Model* model, Object* a, Object* b, float offsetX, float offsetY) {
    Spring& spring = model->m_aSprings[model->m_iNumSprings];
    model->m_iNumSprings++;

    // TODO can't we ctor this?
    // init spring
    spring.m_pA = a;
    spring.m_pB = b;
    spring.m_vOffset.x = offsetX;
    spring.m_vOffset.y = offsetY;
}

void CWobblyModel::calcBounds(Model* model) {
    model->m_vTopLeft.x = std::numeric_limits<short>::max();
    model->m_vTopLeft.y = std::numeric_limits<short>::max();
    model->m_vBottomRight.x = std::numeric_limits<short>::min();
    model->m_vBottomRight.y = std::numeric_limits<short>::min();

    for (int i = 0; i < model->m_iNumObjects; i++) {
        if (model->m_pObjects[i].m_vPosition.x < model->m_vTopLeft.x) {
            model->m_vTopLeft.x = model->m_pObjects[i].m_vPosition.x;
        } else if (model->m_pObjects[i].m_vPosition.x > model->m_vBottomRight.x) {
            model->m_vBottomRight.x = model->m_pObjects[i].m_vPosition.x;
        }
    
        if (model->m_pObjects[i].m_vPosition.y < model->m_vTopLeft.y) {
            model->m_vTopLeft.y = model->m_pObjects[i].m_vPosition.y;
        } else if (model->m_pObjects[i].m_vPosition.y > model->m_vBottomRight.y) {
            model->m_vBottomRight.y = model->m_pObjects[i].m_vPosition.y;
        }
    }
}

int CWobblyModel::modelStep(Model* model, float friction, float k, float time) {
    int   wobblyBits = 0;
    float velocitySum = 0.0f;
    float force, forceSum = 0.0f;

    model->m_fSteps += time / 15.0f;
    int steps = floor (model->m_fSteps);
    model->m_fSteps -= steps;

    if (!steps)
        return 1;

    for (int j = 0; j < steps; j++) {
        for (int i = 0; i < model->m_iNumSprings; i++) {
            springExertForces(&model->m_aSprings[i], k);
        }
    
        for (int i = 0; i < model->m_iNumObjects; i++) {
            velocitySum += objectStep(&model->m_pObjects[i],
                            friction,
                            &force);
            forceSum += force;
        }
    }

    calcBounds(model);

    if (velocitySum > 0.5f)
        wobblyBits |= WobblyVelocity;

    if (forceSum > 20.0f)
        wobblyBits |= WobblyForce;

    return wobblyBits;
}

int CWobblyModel::objectStep(Object* object, float friction, float* force) {
    object->m_fTheta += 0.05f;

    if (object->m_bImmobile) {
        object->m_vVelocity = Vector2D();
        object->m_vForce = Vector2D();

        *force = 0;
        return 0;
    } else {
        // TODO implement operator-= in Vector2D
        object->m_vForce = object->m_vForce - object->m_vVelocity * friction;

        object->m_vVelocity = object->m_vVelocity + object->m_vForce / MASS;

        object->m_vPosition = object->m_vPosition + object->m_vVelocity;

        *force = std::abs(object->m_vForce.x) + std::abs(object->m_vForce.y);
        object->m_vForce = Vector2D();

        return std::abs(object->m_vVelocity.x) + std::abs(object->m_vVelocity.y);
    }
}

void CWobblyModel::springExertForces(Spring* spring, float k) {
    Vector2D a = spring->m_pA->m_vPosition;
    Vector2D b = spring->m_pB->m_vPosition;

    Vector2D deltaA = (b - a - spring->m_vOffset) * 0.5f;
    Vector2D deltaB = (a - b - spring->m_vOffset) * 0.5f;

    objectApplyForce(spring->m_pA, k * deltaA.x, k * deltaA.y);
    objectApplyForce(spring->m_pB, k * deltaB.x, k * deltaB.y);
}

void CWobblyModel::objectApplyForce(Object* object, float forceX, float forceY) {
    object->m_vForce.x += forceX;
    object->m_vForce.y += forceY;
}
