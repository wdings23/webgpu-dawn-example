#include <stdint.h>
#include <math.h>

#ifdef _MSC_VER
#include <corecrt_math_defines.h>
#endif // MSC_VER

#include "mat4.h"
#include "quaternion.h"

#include <float.h>

/*
**
*/
mat4 invert(mat4 const& m)
{
    float inv[16], invOut[16], det;
    int i;
    
    inv[0] = m.mafEntries[5]  * m.mafEntries[10] * m.mafEntries[15] -
    m.mafEntries[5]  * m.mafEntries[11] * m.mafEntries[14] -
    m.mafEntries[9]  * m.mafEntries[6]  * m.mafEntries[15] +
    m.mafEntries[9]  * m.mafEntries[7]  * m.mafEntries[14] +
    m.mafEntries[13] * m.mafEntries[6]  * m.mafEntries[11] -
    m.mafEntries[13] * m.mafEntries[7]  * m.mafEntries[10];
    
    inv[4] = -m.mafEntries[4]  * m.mafEntries[10] * m.mafEntries[15] +
    m.mafEntries[4]  * m.mafEntries[11] * m.mafEntries[14] +
    m.mafEntries[8]  * m.mafEntries[6]  * m.mafEntries[15] -
    m.mafEntries[8]  * m.mafEntries[7]  * m.mafEntries[14] -
    m.mafEntries[12] * m.mafEntries[6]  * m.mafEntries[11] +
    m.mafEntries[12] * m.mafEntries[7]  * m.mafEntries[10];
    
    inv[8] = m.mafEntries[4]  * m.mafEntries[9] * m.mafEntries[15] -
    m.mafEntries[4]  * m.mafEntries[11] * m.mafEntries[13] -
    m.mafEntries[8]  * m.mafEntries[5] * m.mafEntries[15] +
    m.mafEntries[8]  * m.mafEntries[7] * m.mafEntries[13] +
    m.mafEntries[12] * m.mafEntries[5] * m.mafEntries[11] -
    m.mafEntries[12] * m.mafEntries[7] * m.mafEntries[9];
    
    inv[12] = -m.mafEntries[4]  * m.mafEntries[9] * m.mafEntries[14] +
    m.mafEntries[4]  * m.mafEntries[10] * m.mafEntries[13] +
    m.mafEntries[8]  * m.mafEntries[5] * m.mafEntries[14] -
    m.mafEntries[8]  * m.mafEntries[6] * m.mafEntries[13] -
    m.mafEntries[12] * m.mafEntries[5] * m.mafEntries[10] +
    m.mafEntries[12] * m.mafEntries[6] * m.mafEntries[9];
    
    inv[1] = -m.mafEntries[1]  * m.mafEntries[10] * m.mafEntries[15] +
    m.mafEntries[1]  * m.mafEntries[11] * m.mafEntries[14] +
    m.mafEntries[9]  * m.mafEntries[2] * m.mafEntries[15] -
    m.mafEntries[9]  * m.mafEntries[3] * m.mafEntries[14] -
    m.mafEntries[13] * m.mafEntries[2] * m.mafEntries[11] +
    m.mafEntries[13] * m.mafEntries[3] * m.mafEntries[10];
    
    inv[5] = m.mafEntries[0]  * m.mafEntries[10] * m.mafEntries[15] -
    m.mafEntries[0]  * m.mafEntries[11] * m.mafEntries[14] -
    m.mafEntries[8]  * m.mafEntries[2] * m.mafEntries[15] +
    m.mafEntries[8]  * m.mafEntries[3] * m.mafEntries[14] +
    m.mafEntries[12] * m.mafEntries[2] * m.mafEntries[11] -
    m.mafEntries[12] * m.mafEntries[3] * m.mafEntries[10];
    
    inv[9] = -m.mafEntries[0]  * m.mafEntries[9] * m.mafEntries[15] +
    m.mafEntries[0]  * m.mafEntries[11] * m.mafEntries[13] +
    m.mafEntries[8]  * m.mafEntries[1] * m.mafEntries[15] -
    m.mafEntries[8]  * m.mafEntries[3] * m.mafEntries[13] -
    m.mafEntries[12] * m.mafEntries[1] * m.mafEntries[11] +
    m.mafEntries[12] * m.mafEntries[3] * m.mafEntries[9];
    
    inv[13] = m.mafEntries[0]  * m.mafEntries[9] * m.mafEntries[14] -
    m.mafEntries[0]  * m.mafEntries[10] * m.mafEntries[13] -
    m.mafEntries[8]  * m.mafEntries[1] * m.mafEntries[14] +
    m.mafEntries[8]  * m.mafEntries[2] * m.mafEntries[13] +
    m.mafEntries[12] * m.mafEntries[1] * m.mafEntries[10] -
    m.mafEntries[12] * m.mafEntries[2] * m.mafEntries[9];
    
    inv[2] = m.mafEntries[1]  * m.mafEntries[6] * m.mafEntries[15] -
    m.mafEntries[1]  * m.mafEntries[7] * m.mafEntries[14] -
    m.mafEntries[5]  * m.mafEntries[2] * m.mafEntries[15] +
    m.mafEntries[5]  * m.mafEntries[3] * m.mafEntries[14] +
    m.mafEntries[13] * m.mafEntries[2] * m.mafEntries[7] -
    m.mafEntries[13] * m.mafEntries[3] * m.mafEntries[6];
    
    inv[6] = -m.mafEntries[0]  * m.mafEntries[6] * m.mafEntries[15] +
    m.mafEntries[0]  * m.mafEntries[7] * m.mafEntries[14] +
    m.mafEntries[4]  * m.mafEntries[2] * m.mafEntries[15] -
    m.mafEntries[4]  * m.mafEntries[3] * m.mafEntries[14] -
    m.mafEntries[12] * m.mafEntries[2] * m.mafEntries[7] +
    m.mafEntries[12] * m.mafEntries[3] * m.mafEntries[6];
    
    inv[10] = m.mafEntries[0]  * m.mafEntries[5] * m.mafEntries[15] -
    m.mafEntries[0]  * m.mafEntries[7] * m.mafEntries[13] -
    m.mafEntries[4]  * m.mafEntries[1] * m.mafEntries[15] +
    m.mafEntries[4]  * m.mafEntries[3] * m.mafEntries[13] +
    m.mafEntries[12] * m.mafEntries[1] * m.mafEntries[7] -
    m.mafEntries[12] * m.mafEntries[3] * m.mafEntries[5];
    
    inv[14] = -m.mafEntries[0]  * m.mafEntries[5] * m.mafEntries[14] +
    m.mafEntries[0]  * m.mafEntries[6] * m.mafEntries[13] +
    m.mafEntries[4]  * m.mafEntries[1] * m.mafEntries[14] -
    m.mafEntries[4]  * m.mafEntries[2] * m.mafEntries[13] -
    m.mafEntries[12] * m.mafEntries[1] * m.mafEntries[6] +
    m.mafEntries[12] * m.mafEntries[2] * m.mafEntries[5];
    
    inv[3] = -m.mafEntries[1] * m.mafEntries[6] * m.mafEntries[11] +
    m.mafEntries[1] * m.mafEntries[7] * m.mafEntries[10] +
    m.mafEntries[5] * m.mafEntries[2] * m.mafEntries[11] -
    m.mafEntries[5] * m.mafEntries[3] * m.mafEntries[10] -
    m.mafEntries[9] * m.mafEntries[2] * m.mafEntries[7] +
    m.mafEntries[9] * m.mafEntries[3] * m.mafEntries[6];
    
    inv[7] = m.mafEntries[0] * m.mafEntries[6] * m.mafEntries[11] -
    m.mafEntries[0] * m.mafEntries[7] * m.mafEntries[10] -
    m.mafEntries[4] * m.mafEntries[2] * m.mafEntries[11] +
    m.mafEntries[4] * m.mafEntries[3] * m.mafEntries[10] +
    m.mafEntries[8] * m.mafEntries[2] * m.mafEntries[7] -
    m.mafEntries[8] * m.mafEntries[3] * m.mafEntries[6];
    
    inv[11] = -m.mafEntries[0] * m.mafEntries[5] * m.mafEntries[11] +
    m.mafEntries[0] * m.mafEntries[7] * m.mafEntries[9] +
    m.mafEntries[4] * m.mafEntries[1] * m.mafEntries[11] -
    m.mafEntries[4] * m.mafEntries[3] * m.mafEntries[9] -
    m.mafEntries[8] * m.mafEntries[1] * m.mafEntries[7] +
    m.mafEntries[8] * m.mafEntries[3] * m.mafEntries[5];
    
    inv[15] = m.mafEntries[0] * m.mafEntries[5] * m.mafEntries[10] -
    m.mafEntries[0] * m.mafEntries[6] * m.mafEntries[9] -
    m.mafEntries[4] * m.mafEntries[1] * m.mafEntries[10] +
    m.mafEntries[4] * m.mafEntries[2] * m.mafEntries[9] +
    m.mafEntries[8] * m.mafEntries[1] * m.mafEntries[6] -
    m.mafEntries[8] * m.mafEntries[2] * m.mafEntries[5];
    
    det = m.mafEntries[0] * inv[0] + m.mafEntries[1] * inv[4] + m.mafEntries[2] * inv[8] + m.mafEntries[3] * inv[12];
    if(det <= 1.0e-5)
    {
        for(i = 0; i < 16; i++)
            invOut[i] = FLT_MAX;
    }
    else
    {
        det = 1.0f / det;

        for(i = 0; i < 16; i++)
            invOut[i] = inv[i] * det;
    }
    
    return mat4(invOut);
}

/*
**
*/
void mul(mat4* pResult, mat4 const& m0, mat4 const& m1)
{
    for(uint32_t i = 0; i < 4; i++)
    {
        for(uint32_t j = 0; j < 4; j++)
        {
            float fResult = 0.0f;
            for(uint32_t k = 0; k < 4; k++)
            {
                uint32_t iIndex0 = (i << 2) + k;
                uint32_t iIndex1 = (k << 2) + j;
                fResult += (m0.mafEntries[iIndex0] * m1.mafEntries[iIndex1]);
            }
            
            pResult->mafEntries[(i << 2) + j] = fResult;
        }
    }
}

/*
**
*/
void mul(mat4& result, mat4 const& m0, mat4 const& m1)
{
    for(uint32_t i = 0; i < 4; i++)
    {
        for(uint32_t j = 0; j < 4; j++)
        {
            float fResult = 0.0f;
            for(uint32_t k = 0; k < 4; k++)
            {
                uint32_t iIndex0 = (i << 2) + k;
                uint32_t iIndex1 = (k << 2) + j;
                fResult += (m0.mafEntries[iIndex0] * m1.mafEntries[iIndex1]);
            }

            result.mafEntries[(i << 2) + j] = fResult;
        }
    }
}

/*
**
*/
vec4 mul(float4 const& v, mat4 const& m)
{
    return m * v;
}

/*
**
*/
mat4 translate(float fX, float fY, float fZ)
{
    float afVal[16] =
    {
        1.0f, 0.0f, 0.0f, fX,
        0.0f, 1.0f, 0.0f, fY,
        0.0f, 0.0f, 1.0f, fZ,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    
    return mat4(afVal);
}

/*
**
*/
mat4 translate(vec4 const& position)
{
    float afVal[16] =
    {
        1.0f, 0.0f, 0.0f, position.x,
        0.0f, 1.0f, 0.0f, position.y,
        0.0f, 0.0f, 1.0f, position.z,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    
    return mat4(afVal);
}

/*
**
*/
mat4 transpose(mat4 const& m)
{
    float afVal[16] =
    {
        m.mafEntries[0], m.mafEntries[4], m.mafEntries[8], m.mafEntries[12],
        m.mafEntries[1], m.mafEntries[5], m.mafEntries[9], m.mafEntries[13],
        m.mafEntries[2], m.mafEntries[6], m.mafEntries[10], m.mafEntries[14],
        m.mafEntries[3], m.mafEntries[7], m.mafEntries[11], m.mafEntries[15],
    };
    
    return mat4(afVal);
}

/*
**
*/
mat4 mat4::operator * (mat4 const& m) const
{
    float afResults[16];
    
    for(uint32_t i = 0; i < 4; i++)
    {
        for(uint32_t j = 0; j < 4; j++)
        {
            uint32_t iIndex = (i << 2) + j;
            afResults[iIndex] = 0.0f;
            for(uint32_t k = 0; k < 4; k++)
            {
                uint32_t iIndex0 = (i << 2) + k;
                uint32_t iIndex1 = (k << 2) + j;
                afResults[iIndex] += (mafEntries[iIndex0] * m.mafEntries[iIndex1]);
            }
        }
    }
    
    return mat4(afResults);
}

/*
**
*/
mat4 mat4::operator + (mat4 const& m) const
{
    float afResults[16];

    for(uint32_t i = 0; i < 16; i++)
    {
        afResults[i] = mafEntries[i] + m.mafEntries[i];
    }

    return mat4(afResults);
}

/*
**
*/
void mat4::operator += (mat4 const& m)
{
    for(uint32_t i = 0; i < 16; i++)
    {
        mafEntries[i] += m.mafEntries[i];
    }
}

/*
**
*/
mat4 perspectiveProjection(float fFOV,
                           uint32_t iWidth,
                           uint32_t iHeight,
                           float fFar,
                           float fNear)
{
    float fFD = 1.0f / tanf(fFOV * 0.5f);
    float fAspect = (float)iWidth / (float)iHeight;
    float fOneOverAspect = 1.0f / fAspect;
    float fOneOverFarMinusNear = 1.0f / (fFar - fNear);
    
    float afVal[16];
    memset(afVal, 0, sizeof(afVal));
    afVal[0] = fFD * fOneOverAspect;
    afVal[5] = -fFD;
    afVal[10] = -(fFar + fNear) * fOneOverFarMinusNear;
    afVal[14] = -1.0f;
    afVal[11] = -2.0f * fFar * fNear * fOneOverFarMinusNear;
    afVal[15] = 0.0f;
    
#if defined(__APPLE__) || defined(TARGET_IOS)
    afVal[10] = -fFar * fOneOverFarMinusNear;
    afVal[11] = -fFar * fNear * fOneOverFarMinusNear;
#else
#if !defined(GLES_RENDER)
	afVal[5] *= -1.0f;
#endif // GLES_RENDER

#endif // __APPLE__
    
    return mat4(afVal);
}

/*
**
*/
mat4 perspectiveProjectionNegOnePosOne(
    float fFOV,
    uint32_t iWidth,
    uint32_t iHeight,
    float fFar,
    float fNear)
{
    float fFD = 1.0f / tanf(fFOV * 0.5f);
    float fAspect = (float)iWidth / (float)iHeight;
    float fOneOverAspect = 1.0f / fAspect;
    float fOneOverFarMinusNear = 1.0f / (fFar - fNear);

    float afVal[16];
    memset(afVal, 0, sizeof(afVal));
    afVal[0] = fFD * fOneOverAspect;
    afVal[5] = fFD;
    afVal[10] = -(fFar + fNear) * fOneOverFarMinusNear;
    afVal[11] = -1.0f;
    afVal[14] = -2.0f * fFar * fNear * fOneOverFarMinusNear;
    afVal[15] = 0.0f;

    return mat4(afVal);
}

/*
**
*/
mat4 perspectiveProjection2(
    float fFOV,
    uint32_t iWidth,
    uint32_t iHeight,
    float fFar,
    float fNear)
{
    float fFD = 1.0f / tanf(fFOV * 0.5f);
    float fAspect = (float)iWidth / (float)iHeight;
    float fOneOverAspect = 1.0f / fAspect;
    float fOneOverFarMinusNear = 1.0f / (fFar - fNear);

    float afVal[16];
    memset(afVal, 0, sizeof(afVal));
    afVal[0] = -fFD * fOneOverAspect;
    afVal[5] = fFD;
    afVal[10] = -fFar * fOneOverFarMinusNear;
    afVal[11] = fFar * fNear * fOneOverFarMinusNear;
    afVal[14] = -1.0f;
    afVal[15] = 0.0f;

    return mat4(afVal);
}

/*
**
*/
mat4 orthographicProjection(float fLeft,
                            float fRight,
                            float fTop,
                            float fBottom,
                            float fFar,
                            float fNear,
                            bool bInvertY)
{
    float fWidth = fRight - fLeft;
    float fHeight = fTop - fBottom;
    
    float fFarMinusNear = fFar - fNear;
    
    float afVal[16];
    memset(afVal, 0, sizeof(afVal));
    
    afVal[0] = 2.0f / fWidth;
    afVal[3] = -(fRight + fLeft) / (fRight - fLeft);
    afVal[5] = 2.0f / fHeight;
    afVal[7] = -(fTop + fBottom) / (fTop - fBottom);

//#if defined(TARGET_IOS) || defined(__APPLE__)
//    // metal: z -> (0, 1)
//    // vulkan: z -> (0, 1)?
//    
//    afVal[10] = -1.0f / (fFar - fNear);
//    afVal[11] = -fNear / fFarMinusNear;
//#else
//    
//    // opengl: z -> (-1, 1)
//    
//    afVal[10] = -2.0f / fFarMinusNear;
//    afVal[11] = -(fFar + fNear) / fFarMinusNear;
//#endif // TARGET_IOS
    
    afVal[10] = -1.0f / (fFar - fNear);
    afVal[11] = -fNear / fFarMinusNear;

    afVal[15] = 1.0f;
    
    if(bInvertY)
    {
        afVal[5] = -afVal[5];
    }
    
    afVal[0] = -afVal[0];
    
    return mat4(afVal);
}

/*
**
*/
mat4 makeViewMatrix(vec3 const& eyePos, vec3 const& lookAt, vec3 const& up)
{
    vec3 dir = lookAt - eyePos;
    dir = normalize(dir);
    
    vec3 tangent = normalize(cross(up, dir));
    vec3 binormal = normalize(cross(dir, tangent));
    
    float afValue[16] =
    {
        tangent.x, tangent.y, tangent.z, 0.0f,
        binormal.x, binormal.y, binormal.z, 0.0f,
        -dir.x, -dir.y, -dir.z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    
    mat4 xform(afValue);
    
    mat4 translation;
    translation.mafEntries[3] = -eyePos.x;
    translation.mafEntries[7] = -eyePos.y;
    translation.mafEntries[11] = -eyePos.z;
    
    return (xform * translation);
}

/*
**
*/
mat4 makeViewMatrix2(vec3 const& eyePos, vec3 const& lookAt, vec3 const& up)
{
    vec3 dir = lookAt - eyePos;
    dir = normalize(dir);

    vec3 tangent = normalize(cross(up, dir));
    vec3 binormal = normalize(cross(dir, tangent));

    float afValue[16] =
    {
        tangent.x, tangent.y, tangent.z, 0.0f,
        binormal.x, binormal.y, binormal.z, 0.0f,
        dir.x, dir.y, dir.z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };

    mat4 xform(afValue);

    mat4 translation;
    translation.mafEntries[3] = -eyePos.x;
    translation.mafEntries[7] = -eyePos.y;
    translation.mafEntries[11] = -eyePos.z;

    //return (translation * xform);
    return (xform * translation);
}

/*
**
*/
mat4 rotateMatrixX(float fAngle)
{
    float afVal[16] =
    {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, cosf(fAngle), -sinf(fAngle), 0.0f,
        0.0f, sinf(fAngle), cosf(fAngle), 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    
    return mat4(afVal);
}

/*
**
*/
mat4 rotateMatrixY(float fAngle)
{
    float afVal[16] =
    {
        cosf(fAngle), 0.0f, sinf(fAngle), 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        -sinf(fAngle), 0.0f, cosf(fAngle), 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    
    return mat4(afVal);
}

/*
**
*/
mat4 rotateMatrixZ(float fAngle)
{
    float afVal[16] =
    {
        cosf(fAngle), -sinf(fAngle), 0.0f, 0.0f,
        sinf(fAngle), cosf(fAngle), 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    
    return mat4(afVal);
}

/*
**
*/
mat4 scale(float fX, float fY, float fZ)
{
    float afVal[16] =
    {
        fX, 0.0f, 0.0f, 0.0f,
        0.0f, fY, 0.0f, 0.0f,
        0.0f, 0.0f, fZ, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    
    return mat4(afVal);
}

/*
**
*/
mat4 scale(vec4 const& scale)
{
    float afVal[16] =
    {
        scale.x, 0.0f, 0.0f, 0.0f,
        0.0f, scale.y, 0.0f, 0.0f,
        0.0f, 0.0f, scale.z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    
    return mat4(afVal);
}

/*
**
*/
bool mat4::operator == (mat4 const& m) const
{
    bool bRet = true;
    for(uint32_t i = 0; i < 16; i++)
    {
        float fDiff = fabsf(m.mafEntries[i] - mafEntries[i]);
        if(fDiff > 0.0001f)
        {
            bRet = false;
            break;
        }
    }
    
    return bRet;
}

/*
**
*/
vec3 extractEulerAngles(mat4 const& m)
{
	vec3 ret;

	float const fTwoPI = 2.0f * (float)M_PI;

	float fSY = sqrtf(m.mafEntries[0] * m.mafEntries[0] + m.mafEntries[4] * m.mafEntries[4]);   // (0,0), (1,0)

	if(fSY < 1e-6)
	{
		ret.x = atan2f(-m.mafEntries[6], m.mafEntries[5]);     // (1,2) (1,1)
		ret.y = atan2f(-m.mafEntries[8], fSY);      // (2,0)
		ret.z = 0.0f;
}
	else
	{
		ret.x = atan2f(m.mafEntries[9], m.mafEntries[10]);    // (2,1) (2,2)
		ret.y = atan2f(-m.mafEntries[8], fSY);     // (2,0)
		ret.z = atan2f(m.mafEntries[4], m.mafEntries[0]);     // (1,0) (0,0)
	}

	return ret;
}

/*
**
*/
bool mat4::identical(mat4 const& m, float fTolerance) const
{
	bool bRet = true;
	for(uint32_t i = 0; i < 16; i++)
	{
		float fDiff = fabsf(mafEntries[i] - m.mafEntries[i]);
		if(fDiff > fTolerance)
		{
			bRet = false;
			break;
		}
	}

	return bRet;
}

/*
**
*/
mat4 makeAngleAxis(vec3 const& axis, float fAngle)
{
    float fCosAngle = cosf(fAngle);
    float fSinAngle = sinf(fAngle);
    float fT = 1.0f - fCosAngle;

    mat4 m;
    m.mafEntries[0] = fT * axis.x * axis.x + fCosAngle;
    m.mafEntries[5] = fT * axis.y * axis.y + fCosAngle;
    m.mafEntries[10] = fT * axis.z * axis.z + fCosAngle;

    float fTemp0 = axis.x * axis.y * fT;
    float fTemp1 = axis.z * fSinAngle;

    m.mafEntries[4] = fTemp0 + fTemp1;
    m.mafEntries[1] = fTemp0 - fTemp1;

    fTemp0 = axis.x * axis.z * fT;
    fTemp1 = axis.y * fSinAngle;

    m.mafEntries[8] = fTemp0 - fTemp1;
    m.mafEntries[2] = fTemp0 + fTemp1;

    fTemp0 = axis.y * axis.z * fT;
    fTemp1 = axis.x * fSinAngle;

    m.mafEntries[9] = fTemp0 + fTemp1;
    m.mafEntries[6] = fTemp0 - fTemp1;
    

    return m;
}
