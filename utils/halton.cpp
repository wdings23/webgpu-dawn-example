#include "halton.h"

namespace Utils
{
    /*
    **
    */
    float halton(int index, int base)
    {
        float f = 1.0f;
        float r = 0.0f;
        while(index > 0) {
            f = f / base;
            r = r + f * (index % base);
            index = index / base;
        }
        return r;
    }

    /*
    **
    */
    float2 halton_2d(int index)
    {
        return float2(Utils::halton(index, 2), Utils::halton(index, 3));
    }

    /*
    **
    */
    float2 get_jitter_offset(int frame_index, int width, int height)
    {
        float2 halton_sample = Utils::halton_2d(frame_index);
        // Scale and translate to center around pixel center and limit to pixel range
        float x = ((halton_sample.x - 0.5f) / width) * 2.0f;
        float y = ((halton_sample.y - 0.5f) / height) * 2.0f;
        return float2(x, y);
    }

}   // Utils