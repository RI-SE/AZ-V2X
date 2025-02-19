#include "geo_utils.hpp"
#include <cmath>

std::string calculateQuadTree(double lat, double lon) {
    std::string quadTree;
    quadTree.reserve(18);  // We need exactly 18 characters

    // Normalize coordinates to [0,1]
    double x = (lon + 180.0) / 360.0;
    double y = (lat + 90.0) / 180.0;

    for (int i = 0; i < 18; i++) {
        double rx = 0, ry = 0;
        int digit = 0;

        rx = fmod(x * 2, 1.0);
        ry = fmod(y * 2, 1.0);
        x *= 2;
        y *= 2;

        if (rx >= 0.5) {
            digit |= 1;
            x -= 0.5;
        }
        if (ry >= 0.5) {
            digit |= 2;
            y -= 0.5;
        }

        quadTree += std::to_string(digit);
    }

    return quadTree;
} 