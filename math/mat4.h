#pragma once

#include "vec.h"

#include <stdint.h>
#include <string.h>

#define _m00    mafEntries[0]
#define _m01    mafEntries[1]
#define _m02    mafEntries[2]
#define _m03    mafEntries[3]

#define _m10    mafEntries[4]
#define _m11    mafEntries[5]
#define _m12    mafEntries[6]
#define _m13    mafEntries[7]

#define _m20    mafEntries[8]
#define _m21    mafEntries[9]
#define _m22    mafEntries[10]
#define _m23    mafEntries[11]

#define _m30    mafEntries[12]
#define _m31    mafEntries[13]
#define _m32    mafEntries[14]
#define _m33    mafEntries[15]

#define __11    mafEntries[0]
#define __12    mafEntries[4]
#define __13    mafEntries[8]
#define __14    mafEntries[12]
         
#define __21    mafEntries[1]
#define __22    mafEntries[5]
#define __23    mafEntries[9]
#define __24    mafEntries[13]
         
#define __31    mafEntries[2]
#define __32    mafEntries[6]
#define __33    mafEntries[10]
#define __34    mafEntries[14]
         
#define __41    mafEntries[3]
#define __42    mafEntries[7]
#define __43    mafEntries[11]
#define __44    mafEntries[15]

/*
**
*/
struct mat4
{
    mat4()
    {
        memset(mafEntries, 0, sizeof(mafEntries));
        mafEntries[0] = mafEntries[5] = mafEntries[10] = mafEntries[15] = 1.0f;
    }
    
    mat4(float const* afEntries)
    {
        memcpy(mafEntries, afEntries, sizeof(mafEntries));
    }
    
    mat4(vec3 const& row0, vec3 const& row1, vec3 const& row2)
    {
        mafEntries[0] = row0.x; mafEntries[1] = row0.y; mafEntries[2] = row0.z; mafEntries[3] = 0.0f;
        mafEntries[4] = row1.x; mafEntries[5] = row1.y; mafEntries[6] = row1.z; mafEntries[7] = 0.0f;
        mafEntries[8] = row2.x; mafEntries[9] = row2.y; mafEntries[10] = row2.z; mafEntries[11] = 0.0f;
        mafEntries[12] = 0.0f; mafEntries[13] = 0.0f; mafEntries[14] = 0.0f; mafEntries[15] = 1.0f;
    }
    
    vec3 operator * (vec3 const& v) const
    {
        float fX = v.x * mafEntries[0] + v.y * mafEntries[1] + v.z * mafEntries[2] + mafEntries[3];
        float fY = v.x * mafEntries[4] + v.y * mafEntries[5] + v.z * mafEntries[6] + mafEntries[7];
        float fZ = v.x * mafEntries[8] + v.y * mafEntries[9] + v.z * mafEntries[10] + mafEntries[11];
        
        return vec3(fX, fY, fZ);
    }
    
    vec4 operator * (vec4 const& v) const
    {
        float fX = v.x * mafEntries[0] + v.y * mafEntries[1] + v.z * mafEntries[2] + v.w * mafEntries[3];
        float fY = v.x * mafEntries[4] + v.y * mafEntries[5] + v.z * mafEntries[6] + v.w * mafEntries[7];
        float fZ = v.x * mafEntries[8] + v.y * mafEntries[9] + v.z * mafEntries[10] + v.w * mafEntries[11];
        float fW = v.x * mafEntries[12] + v.y * mafEntries[13] + v.z * mafEntries[14] + v.w * mafEntries[15];
        
        return vec4(fX, fY, fZ, fW);
    }
    
    mat4 operator + (mat4 const& m) const;

    mat4 operator * (mat4 const& m) const;
    
    void operator += (mat4 const& m);
    bool operator == (mat4 const& m) const;
    
    void identity() {memset(mafEntries, 0, sizeof(mafEntries)); mafEntries[0] = mafEntries[5] = mafEntries[10] = mafEntries[15] = 1.0f; }

	bool identical(mat4 const& m, float fTolerance) const;

    float   mafEntries[16];
};

mat4 makeViewMatrix(vec3 const& eyePos, vec3 const& lookAt, vec3 const& up);
mat4 makeViewMatrix2(vec3 const& eyePos, vec3 const& lookAt, vec3 const& up);

mat4 perspectiveProjection(float fFOV,
                           uint32_t iWidth,
                           uint32_t iHeight,
                           float fFar,
                           float fNear);

mat4 perspectiveProjection2(
    float fFOV,
    uint32_t iWidth,
    uint32_t iHeight,
    float fFar,
    float fNear);

mat4 perspectiveProjectionNegOnePosOne(
    float fFOV,
    uint32_t iWidth,
    uint32_t iHeight,
    float fFar,
    float fNear);

mat4 orthographicProjection(float fLeft,
                            float fRight,
                            float fTop,
                            float fBottom,
                            float fFar,
                            float fNear,
                            bool bInvertY = false);

mat4 translate(float fX, float fY, float fZ);
mat4 translate(vec4 const& position);

void mul(mat4* pResult, mat4 const& m0, mat4 const& m1);
void mul(mat4& result, mat4 const& m0, mat4 const& m1);
vec4 mul(float4 const& v, mat4 const& m);

mat4 invert(mat4 const& m);
mat4 transpose(mat4 const& m);

mat4 rotateMatrixX(float fAngle);
mat4 rotateMatrixY(float fAngle);
mat4 rotateMatrixZ(float fAngle);

mat4 scale(float fX, float fY, float fZ);
mat4 scale(vec4 const& scale);

vec3 extractEulerAngles(mat4 const& m);

mat4 makeAngleAxis(vec3 const& axis, float fAngle);

typedef mat4 float4x4;