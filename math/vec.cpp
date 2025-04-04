#include <math.h>
#include <algorithm>
#include <assert.h>
#include "vec.h"

/*
**
*/
vec3::vec3(vec4 const& v)
{
    x = v.x; y = v.y; z = v.z;
}

/*
**
*/
float dot(vec2 const& v0, vec2 const& v1)
{
    return v0.x * v1.x + v0.y * v1.y;
}

/*
**
*/
float dot(vec3 const& v0, vec3 const& v1)
{
    return v0.x * v1.x + v0.y * v1.y + v0.z * v1.z;
}

/*
**
*/
float dot(vec4 const& v0, vec4 const& v1)
{
	return v0.x * v1.x + v0.y * v1.y + v0.z * v1.z + v0.w * v1.w;
}

/*
**
*/
vec3 cross(vec3 const& v0, vec3 const& v1)
{
    float fX = v0.y * v1.z - v0.z * v1.y;
    float fY = v0.z * v1.x - v0.x * v1.z;
    float fZ = v0.x * v1.y - v0.y * v1.x;
    
    return vec3(fX, fY, fZ);
}

/*
**
*/
vec3 antiCross(vec3 const& v0, vec3 const& v1)
{
    
    float fX = v0.z * v1.y - v1.y * v0.z;
    float fY = v0.x * v1.z - v1.z * v0.x;
    float fZ = v0.y * v1.x - v1.x * v0.y;

    return vec3(fX, fY, fZ);
}

/*
**
*/
vec2 normalize(vec2 const& v)
{
    float fLength = (float)sqrtf(v.x * v.x + v.y * v.y);
    return vec2(v.x / fLength, v.y / fLength);
}

/*
**
*/
vec3 normalize(vec3 const& v)
{
    float fLength = (float)sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    return vec3(v.x / fLength, v.y / fLength, v.z / fLength);
}

/*
**
*/
vec4 normalize(vec4 const& v)
{
    float fLength = (float)sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    return vec4(v.x / fLength, v.y / fLength, v.z / fLength, 1.0f);
}

/*
**
*/
float length(vec3 const& v)
{
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

/*
**
*/
float length(vec4 const& v)
{
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

/*
**
*/
float length(vec2 const& v)
{
    return sqrtf(v.x * v.x + v.y * v.y);
}

/*
**
*/
float lengthSquared(vec3 const& v)
{
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

/*
**
*/
float lengthSquared(vec4 const& v)
{
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

/*
**
*/
float lengthSquared(vec2 const& v)
{
    return v.x * v.x + v.y * v.y;
}

/*
**
*/
vec3 reflect(vec3 const& v, vec3 const& normal)
{
    vec3 r = v - (normal * 2.0f * dot(v, normal));
    
    return r;
}

/*
**
*/
float minf(float fNum0, float fNum1)
{
    return fNum0 < fNum1 ? fNum0 : fNum1;
}

/*
**
*/
float maxf(float fNum0, float fNum1)
{
    return fNum0 > fNum1 ? fNum0 : fNum1;
}

/*
**
*/
float3 fminf(vec3 const& v0, vec3 const& v1)
{
    vec3 ret = v0;
    ret.x = (v1.x < ret.x) ? v1.x : ret.x;
    ret.y = (v1.y < ret.y) ? v1.y : ret.y;
    ret.z = (v1.z < ret.z) ? v1.z : ret.z;

    return ret;
}

/*
**
*/
vec3 fmaxf(vec3 const& v0, vec3 const& v1)
{
    vec3 ret = v0;
    ret.x = (v1.x > ret.x) ? v1.x : ret.x;
    ret.y = (v1.y > ret.y) ? v1.y : ret.y;
    ret.z = (v1.z > ret.z) ? v1.z : ret.z;

    return ret;
}

/*
**
*/
int32_t clamp(int32_t v, int32_t iMin, int32_t iMax)
{
    return std::min(iMax, std::max(v, iMin));
}

/*
**
*/
uint32_t clamp(uint32_t v, uint32_t iMin, uint32_t iMax)
{
    return std::min(iMax, std::max(v, iMin));
}

/*
**
*/
float clamp(float v, float fMin, float fMax)
{
    assert(fMax >= fMin);
    return minf(fMax, maxf(v, fMin));
}

/*
**
*/
vec2 clamp(vec2 const& v, float fMin, float fMax)
{
    vec2 ret = v;
    ret.x = minf(fMax, maxf(v.x, fMin));
    ret.y = minf(fMax, maxf(v.y, fMin));

    return ret;
}

/*
**
*/
vec3 clamp(vec3 const& v, float fMin, float fMax)
{
    vec3 ret = v;
    ret.x = minf(fMax, maxf(v.x, fMin));
    ret.y = minf(fMax, maxf(v.y, fMin));
    ret.z = minf(fMax, maxf(v.z, fMin));

    return ret;
}

/*
**
*/
vec4 clamp(vec4 const& v, float fMin, float fMax)
{
    vec4 ret = v;
    ret.x = minf(fMax, maxf(v.x, fMin));
    ret.y = minf(fMax, maxf(v.y, fMin));
    ret.z = minf(fMax, maxf(v.z, fMin));
    ret.w = minf(fMax, maxf(v.w, fMin));

    return ret;
}

/*
**
*/
vec2 clamp(vec2 const& v, vec2 const& min, vec2 const& max)
{
    vec2 ret = v;
    ret.x = minf(max.x, maxf(v.x, min.x));
    ret.y = minf(max.y, maxf(v.y, min.y));

    return ret;
}

/*
**
*/
vec3 clamp(vec3 const& v, vec3 const& min, vec3 const& max)
{
    vec3 ret = v;
    ret.x = minf(max.x, maxf(v.x, min.x));
    ret.y = minf(max.y, maxf(v.y, min.y));
    ret.z = minf(max.z, maxf(v.z, min.z));

    return ret;
}

/*
**
*/
vec4 clamp(vec4 const& v, vec4 const& min, vec4 const& max)
{
    vec4 ret = v;
    ret.x = minf(max.x, maxf(v.x, min.x));
    ret.y = minf(max.y, maxf(v.y, min.y));
    ret.z = minf(max.z, maxf(v.z, min.z));
    ret.w = minf(max.w, maxf(v.w, min.w));

    return ret;
}

/*
**
*/
float lerp(
    float v0,
    float v1,
    float fStep)
{
    return v0 + ((v1 - v0) * fStep);
}

/*
**
*/
vec2 lerp(
    vec2 const& v0,
    vec2 const& v1,
    float fStep)
{
    return v0 + ((v1 - v0) * fStep);
}

/*
**
*/
vec3 lerp(
    vec3 const& v0,
    vec3 const& v1,
    float fStep)
{
    return v0 + ((v1 - v0) * fStep);
}

/*
**
*/
vec4 lerp(
    vec4 const& v0,
    vec4 const& v1,
    float fStep)
{
    return v0 + ((v1 - v0) * fStep);
}

/*
**
*/
vec2 mix(
    vec2 const& v0,
    vec2 const& v1,
    float fPct)
{
    return v0 * fPct + v1 * (1.0f - fPct);
}

/*
**
*/
vec3 mix(
    vec3 const& v0,
    vec3 const& v1,
    float fPct)
{
    return v0 * fPct + v1 * (1.0f - fPct);
}

/*
**
*/
vec4 mix(
    vec4 const& v0,
    vec4 const& v1,
    float fPct)
{
    return v0 * fPct + v1 * (1.0f - fPct);
}

/*
**
*/
vec3 maxf(vec3 const& v0, vec3 const& v1)
{
    return vec3(
        maxf(v0.x, v1.x),
        maxf(v0.y, v1.y),
        maxf(v0.z, v1.z));
}

/*
**
*/
vec3 floor(vec3 const& v)
{
    return vec3(floorf(v.x), floorf(v.y), floorf(v.z));
}

/*
**
*/
vec4 floor(vec4 const& v)
{
    return vec4(floorf(v.x), floorf(v.y), floorf(v.z), floorf(v.w));
}

/*
**
*/
vec3 ceil(vec3 const& v)
{
    return vec3(ceilf(v.x), ceilf(v.y), ceilf(v.z));
}

/*
**
*/
vec4 ceil(vec4 const& v)
{
    return vec4(ceilf(v.x), ceilf(v.y), ceilf(v.z), ceilf(v.w));
}

/*
**
*/
vec3 abs(vec3 const& v)
{
    return vec3(fabsf(v.x), fabsf(v.y), fabsf(v.z));
}

/*
**
*/
vec4 abs(vec4 const& v)
{
    return vec4(fabsf(v.x), fabsf(v.y), fabsf(v.z), fabsf(v.w));
}

/*
**
*/
vec3 sign(vec3 const& v)
{
    return vec3(v.x < 0.0f ? -1.0f : 1.0f, v.y < 0.0f ? -1.0f : 1.0f, v.z < 0.0f ? -1.0f : 1.0f);
}

/*
**
*/
vec4 sign(vec4 const& v)
{
    return vec4(v.x < 0.0f ? -1.0f : 1.0f, v.y < 0.0f ? -1.0f : 1.0f, v.z < 0.0f ? -1.0f : 1.0f, v.z < 0.0f ? -1.0f : 1.0f);
}

/*
**
*/
vec3 pow(vec3 const& v, float fVal)
{
    return vec3(
        powf(v.x, fVal),
        powf(v.y, fVal),
        powf(v.x, fVal));
}

vec4 pow(vec4 const& v, float fVal)
{
    return vec4(
        powf(v.x, fVal),
        powf(v.y, fVal),
        powf(v.x, fVal),
        powf(v.w, fVal));
}

/*
**
*/
vec3 saturate(vec3 const& v)
{
    return clamp(v, 0.0f, 1.0f);
}

/*
**
*/
vec4 saturate(vec4 const& v)
{
    return clamp(v, 0.0f, 1.0f);
}

/*
**
*/
float step(float y, float x)
{
    return (x >= y) ? 1.0f : 0.0f;
}

/*
**
*/
float smoothstep(float min, float max, float x)
{
    float fRet = 0.0f;
    if(x < min)
    {
        fRet = 0.0f;
    }
    else if(x > max)
    {
        fRet = 1.0f;
    }
    else
    {
        // temp linear interpolation for now
        fRet = min + (max - min) * x;
    }

    return fRet;
}

/*
**
*/
int3 imin(int3 const& v, int32_t iVal)
{
    int3 ret = v;

    ret.x = (iVal < ret.x) ? iVal : ret.x;
    ret.y = (iVal < ret.y) ? iVal : ret.y;
    ret.z = (iVal < ret.z) ? iVal : ret.z;

    return ret;
}

/*
**
*/
int3 imin(int3 const& v0, int3 const& v1)
{
    int3 ret = v0;

    ret.x = (v1.x < ret.x) ? v1.x : ret.x;
    ret.y = (v1.y < ret.y) ? v1.y : ret.y;
    ret.z = (v1.z < ret.z) ? v1.z : ret.z;

    return ret;
}

/*
**
*/
int3 imax(int3 const& v, int32_t iVal)
{
    int3 ret = v;

    ret.x = (iVal > ret.x) ? iVal : ret.x;
    ret.y = (iVal > ret.y) ? iVal : ret.y;
    ret.z = (iVal > ret.z) ? iVal : ret.z;

    return ret;
}

/*
**
*/
int3 imax(int3 const& v0, int3 const& v1)
{
    int3 ret = v0;

    ret.x = (v1.x > ret.x) ? v1.x : ret.x;
    ret.y = (v1.y > ret.y) ? v1.y : ret.y;
    ret.z = (v1.z > ret.z) ? v1.z : ret.z;

    return ret;
}

/*
**
*/
vec3 vceilf(vec3 const& v)
{
    return vec3(
        ceilf(v.x),
        ceilf(v.y),
        ceilf(v.z));
}

/*
**
*/
vec4 vceilf(vec4 const& v)
{
    return vec4(
        ceilf(v.x),
        ceilf(v.y),
        ceilf(v.z),
        ceilf(v.w));
}

/*
**
*/
vec3 vfloorf(vec3 const& v)
{
    return vec3(
        floorf(v.x),
        floorf(v.y),
        floorf(v.z));
}

/*
**
*/
vec4 vfloorf(vec4 const& v)
{
    return vec4(
        floorf(v.x),
        floorf(v.y),
        floorf(v.z),
        floorf(v.w));
}

/*
**
*/
float frac(float v)
{
    return v - float(int32_t(v));
}

/*
**
*/
vec2 frac(vec2 const& v)
{
    return vec2(
        v.x - float(int32_t(v.x)),
        v.y - float(int32_t(v.y)));
}

/*
**
*/
vec3 frac(vec3 const& v)
{
    return vec3(
        v.x - float(int32_t(v.x)),
        v.y - float(int32_t(v.y)),
        v.z - float(int32_t(v.z)));
}

/*
**
*/
vec4 frac(vec4 const& v)
{
    return vec4(
        v.x - floorf(v.x),
        v.y - floorf(v.y),
        v.z - floorf(v.z),
        v.w - floorf(v.w));
}

/*
**
*/
vec2 vfabsf(vec2 const& v)
{
    return vec2(
        fabsf(v.x),
        fabsf(v.y));
}

/*
**
*/
vec3 vfabsf(vec3 const& v)
{
    return vec3(
        fabsf(v.x),
        fabsf(v.y),
        fabsf(v.z));
}

/*
**
*/
vec4 vfabsf(vec4 const& v)
{
    return vec4(
        fabsf(v.x),
        fabsf(v.y),
        fabsf(v.z),
        fabsf(v.w));
}