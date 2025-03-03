#include "geo_utils.hpp"
#include <cmath>

std::string calculateQuadTree(double lat, double lon, int zoom) {
	// Convert latitude to sinusoidal projection (Mercator projection)
	double sinlat = std::sin(lat * M_PI / 180.0);
	
	// Calculate normalized x and y coordinates
	double x = 0.5 + lon / 360.0;
	double y = 0.5 - std::log((1.0 + sinlat) / (1.0 - sinlat)) / (4 * M_PI);
	
	// Scale to zoom level
	x = std::floor(std::pow(2, zoom) * x);
	y = std::floor(std::pow(2, zoom) * y);
	
	// Build quadtree string
	std::string quadTree;
	quadTree.reserve(zoom);
	
	for (int bn = 0; bn < zoom; bn++) {
		// Cast to int before bitwise operations
		int ix = static_cast<int>(x);
		int iy = static_cast<int>(y);
		
		// Add digit at the beginning of the string (prepend)
		quadTree = std::to_string((ix & 1) | (2 * (iy & 1))) + quadTree;
		
		// Bit shift coordinates
		x = x / 2;  // Division instead of bit shift for doubles
		y = y / 2;  // Division instead of bit shift for doubles
	}
	
	return quadTree;
}
