#pragma once

#include <stdint.h>
#include <math/vec.h>
#include <math/mat4.h>

enum ProjectionType
{
    PROJECTION_PERSPECTIVE = 0,
    PROJECTION_ORTHOGRAPHIC,

    NUM_PROJECTION_TYPES,
};

struct CameraUpdateInfo
{
    float        mfViewWidth;
    float        mfViewHeight;

    float        mfFieldOfView;

    vec3         mUp;

    float        mfNear;
    float        mfFar;

    float2       mProjectionJitter = float2(0.0f, 0.0f);
};

enum
{
    FRUSTUM_PLANE_LEFT = 0,
    FRUSTUM_PLANE_RIGHT,
    FRUSTUM_PLANE_TOP,
    FRUSTUM_PLANE_BOTTOM,
    FRUSTUM_PLANE_NEAR,
    FRUSTUM_PLANE_FAR,

    NUM_FRUSTUM_PLANES,
};

class CCamera
{
public:
    CCamera();
    virtual ~CCamera();

    void update(CameraUpdateInfo& info);

    inline mat4 const& getViewMatrix() const { return mViewMatrix; }
    inline mat4 const& getProjectionMatrix() const { return mProjectionMatrix; }
    inline vec3 const& getPosition() const { return mPosition; }
    inline vec3 const& getLookAt() const { return mLookAt; }
    inline float getFar() const { return mfFar; }
    inline float getNear() const { return mfNear; }

    void setLookAt(vec3 const& lookAt) { mLookAt = lookAt; mbDebug = true; }
    void setPosition(vec3 const& position) { mPosition = position; mbDebug = true; }
    void setFar(float fFar) { mfFar = fFar; }
    void setNear(float fNear) { mfNear = fNear; }
    void setProjectionType(ProjectionType const& type) { mProjectionType = type; }

    inline void setViewProjectionMatrix(mat4 const& matrix) { mViewProjectionMatrix = matrix; }

    bool isBoxInFrustum(vec3 const& topLeftFront, vec3 const& bottomRightBack) const;
    inline mat4 const& getViewProjectionMatrix() const { return mViewProjectionMatrix; }

    inline mat4 const& getJitterProjectionMatrix() const { return mJitterProjectionMatrix; }
    inline mat4 const& getJitterViewProjectionMatrix() const { return mJitterViewProjectionMatrix; }

    inline vec4 const& getFrustumPlane(uint32_t iPlane)
    {
        return maFrustomPlanes[iPlane];
    }

    bool                    mbDebug;

protected:
    vec3                    mPosition;
    vec3                    mLookAt;

    float                   mfFar;
    float                   mfNear;

    mat4                    mViewMatrix;
    mat4                    mProjectionMatrix;
    mat4                    mViewProjectionMatrix;

    mat4                    mJitterProjectionMatrix;
    mat4                    mJitterViewProjectionMatrix;

    ProjectionType          mProjectionType;

    vec4                    maFrustomPlanes[NUM_FRUSTUM_PLANES];
};
