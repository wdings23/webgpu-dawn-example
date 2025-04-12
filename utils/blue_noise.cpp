#pragma once

#include <utils/blue_noise.h>

#include <random>
#include <cmath>

#define PI 3.14159f

namespace Utils
{
    void insertPointIntoGrid(
        std::vector<int>& grid,
        float x,
        float y,
        int pointIndex,
        float gridSize,
        int gridWidth);

    bool isNearExistingPoint(
        std::vector<int> const& grid,
        float x,
        float y,
        float gridSize,
        float gridWidth,
        float gridHeight,
        float minDistance);

    /*
    **
    */
    std::vector<std::pair<float, float>> generatePoints(
        float minDistance,
        int width,
        int height,
        int maxAttempts)
    {
        std::vector<std::pair<float, float>> points;
        std::vector<int> activeList;

        std::uniform_real_distribution<float> distribution;
        std::mt19937 generator;

        std::vector<int> grid(width * height, -1);
        float gridSize = minDistance / sqrtf(2.0f);

        // Initialize with a random point
        float x = distribution(generator) * width;
        float y = distribution(generator) * height;
        points.push_back({x, y});
        activeList.push_back((int)points.size() - 1);
        Utils::insertPointIntoGrid(
            grid,
            x,
            y,
            (int)points.size() - 1,
            gridSize,
            width);

        while(!activeList.empty()) {
            int randomIndex = std::uniform_int_distribution<int>(0, (int)activeList.size() - 1)(generator);
            int pointIndex = activeList[randomIndex];
            float px = points[pointIndex].first;
            float py = points[pointIndex].second;

            bool found = false;
            for(int i = 0; i < maxAttempts; ++i) {
                float angle = distribution(generator) * 2 * PI;
                float radius = minDistance + distribution(generator) * minDistance;
                float nx = px + radius * std::cos(angle);
                float ny = py + radius * std::sin(angle);

                bool nearExistingPoint = Utils::isNearExistingPoint(
                    grid,
                    nx,
                    ny,
                    gridSize,
                    (float)width,
                    (float)height,
                    minDistance);

                if(nx >= 0 && nx < width && ny >= 0 && ny < height && !nearExistingPoint) {
                    points.push_back({nx, ny});
                    Utils::insertPointIntoGrid(
                        grid,
                        nx,
                        ny,
                        (int)points.size() - 1,
                        gridSize,
                        width);
                    activeList.push_back((int)points.size() - 1);
                    found = true;
                    break;
                }
            }

            if(!found) {
                activeList.erase(activeList.begin() + randomIndex);
            }
        }
        return points;
    }

    /*
    **
    */
    int getGridIndex(
        float x,
        float y,
        float gridSize,
        int gridWidth) {
        int gridX = static_cast<int>(x / gridSize);
        int gridY = static_cast<int>(y / gridSize);
        return gridY * gridWidth + gridX;
    }

    /*
    **
    */
    void insertPointIntoGrid(
        std::vector<int>& grid,
        float x,
        float y,
        int pointIndex,
        float gridSize,
        int gridWidth)
    {
        int index = Utils::getGridIndex(
            x,
            y,
            gridSize,
            gridWidth);
        grid[index] = pointIndex;
    }

    /*
    **
    */
    bool isNearExistingPoint(
        std::vector<int> const& grid,
        float x,
        float y,
        float gridSize,
        float gridWidth,
        float gridHeight,
        float minDistance)
    {
        int gridX = static_cast<int>(x / gridSize);
        int gridY = static_cast<int>(y / gridSize);
        int startX = std::max(0, gridX - 2);
        int endX = std::min(int(gridWidth - 1), gridX + 2);
        int startY = std::max(0, gridY - 2);
        int endY = std::min(int(gridHeight - 1), gridY + 2);

        for(int i = startY; i <= endY; ++i) {
            for(int j = startX; j <= endX; ++j) {
                int pointIndex = grid[i * (int)gridWidth + j];
                if(pointIndex != -1) {
                    float dx = x - (float)(j * gridSize + gridSize / 2.0f);
                    float dy = y - (float)(i * gridSize + gridSize / 2.0f);
                    if(std::sqrt(dx * dx + dy * dy) < minDistance) {
                        return true;
                    }
                }
            }
        }
        return false;
    }
}   // Utils