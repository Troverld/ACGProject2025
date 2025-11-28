#pragma once

#include <cmath>
#include <limits>
#include <memory>
#include <random>
#include <algorithm>

// Common Headers
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

/**
 * @brief Global constants used throughout the renderer.
 */
const float Infinity = std::numeric_limits<float>::infinity();
const float PI = glm::pi<float>();

// Math tolerance for checking zero, etc. (1e-6 is safer for float than 1e-8)
const float EPSILON = 1e-6f;

// Ray intersection bias to prevent shadow acne (self-intersection)
const float SHADOW_EPSILON = 1e-3f;

/**
 * @brief Generates a random float in range [0.0, 1.0).
 * 
 * @return float Random value between 0.0 (inclusive) and 1.0 (exclusive).
 */
inline float random_float() {
    static thread_local std::mt19937 generator;
    std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
    return distribution(generator);
}

/**
 * @brief Generates a random float in range [min, max).
 * 
 * @param min Minimum value.
 * @param max Maximum value.
 * @return float Random value between min and max.
 */
inline float random_float(float min, float max) {
    return min + (max - min) * random_float();
}

/**
 * @brief Generates a random vector with components in range [min, max).
 */
inline glm::vec3 random_vec3(float min, float max) {
    return glm::vec3(random_float(min, max), random_float(min, max), random_float(min, max));
}

/**
 * @brief Generates a random vector inside a unit sphere.
 * Used for diffuse bounce calculations.
 * 
 * @return glm::vec3 A vector with length < 1.
 */
inline glm::vec3 random_in_unit_sphere() {
    while (true) {
        glm::vec3 p = random_vec3(-1, 1);
        if (glm::dot(p, p) >= 1) continue;
        return p;
    }
}

/**
 * @brief Generates a random unit vector (normalized).
 * Represents a random direction on the surface of a unit sphere.
 * Lambertian distribution approximation.
 * 
 * @return glm::vec3 Normalized random vector.
 */
inline glm::vec3 random_unit_vector() {
    return glm::normalize(random_in_unit_sphere());
}

/**
 * @brief Checks if a vector is near zero in all dimensions.
 * Used to prevent NaNs when a reflection vector is zero.
 */
inline bool near_zero(const glm::vec3& v) {
    return (std::fabs(v.x) < EPSILON) && (std::fabs(v.y) < EPSILON) && (std::fabs(v.z) < EPSILON);
}