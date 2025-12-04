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