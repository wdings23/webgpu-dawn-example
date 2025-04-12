#pragma once

#include <vector>

namespace Utils
{
    std::vector<std::pair<float, float>> generatePoints(
        float minDistance,
        int width,
        int height,
        int maxAttempts = 30);

};  // Utils