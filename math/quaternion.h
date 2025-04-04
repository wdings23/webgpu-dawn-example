#pragma once

#include "vec.h"
#include "mat4.h"

struct mat4;

struct quaternion
{
    float   x;
    float   y;
    float   z;
    float   w;
    
    quaternion() { x = y = z = 0.0f; w = 1.0f; }
    
    quaternion(float fX, float fY, float fZ, float fW)
    {
        x = fX; y = fY; z = fZ; w = fW;
    }
    
    quaternion operator + (quaternion const& quat)
    {
        return quaternion(quat.x + x, quat.y + y, quat.z + z, quat.w + w);
    }
    
    quaternion operator - (quaternion const& quat)
    {
        return quaternion(x - quat.x, y - quat.y, z - quat.z, w - quat.w);
    }
    
    quaternion operator * (quaternion const& quat)
    {
        return quaternion(
            w * quat.x + x * quat.w + y * quat.z - z * quat.y,
            w * quat.y + y * quat.w + z * quat.x - x * quat.z,
            w * quat.z + z * quat.w + x * quat.y - y * quat.x,
            w * quat.w - x * quat.x - y * quat.y - z * quat.z
        );
    }
    
    quaternion fromAngleAxis(vec3 const& axis, float fAngle)
    {
        vec3 scalar = axis * sinf(fAngle * 0.5f);
        return quaternion(
            scalar.x,
            scalar.y,
            scalar.z,
            cosf(fAngle * 0.5f)
        );
    }
    
    vec4 toAngleAxis()
    {
        float fDenom = sqrtf(1.0f - w * w);
        vec4 ret(x, y, z, 0.0f);
        if(fDenom > 0.00001f)
        {
            ret /= fDenom;
        }
        
        ret.w = 2.0f * acosf(w);
        
        return ret;
    }
    
    mat4 matrix();

	quaternion fromMatrix(mat4 const& mat);
	vec3 toEuler();
};


