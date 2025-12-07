#pragma once
#include <glm/glm.hpp>

/**
 * @brief Represents a single photon in the scene.
 * Used for Photon Mapping.
 */
struct Photon {
    glm::vec3 p;        // Position of the photon hit
    glm::vec3 power;    // Flux (Power) carried by the photon
    glm::vec3 incoming; // Direction from which the photon arrived (normalized)
    
    // KD-Tree construction flag
    // 0: x-axis, 1: y-axis, 2: z-axis
    short plane;
};

/**
 * @brief Used for k-Nearest Neighbor in Photon Mapping.
 */
struct NearPhoton {
    const Photon* photon;
    float dist_sq;

    // We want the photon with the largest distance at the top.
    bool operator<(const NearPhoton& other) const {
        return dist_sq < other.dist_sq;
    }
};