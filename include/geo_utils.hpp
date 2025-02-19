#ifndef GEO_UTILS_HPP
#define GEO_UTILS_HPP

#include <string>

// Calculate quadTree value for given lat/lon coordinates
// Returns an 18-character quadTree string
std::string calculateQuadTree(double lat, double lon);

#endif // GEO_UTILS_HPP 