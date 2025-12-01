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
 * @brief Global constants used throughout the renderer. No 'magic numbers' shall appear throughout the project, all of them shall be explicitly defined here.
 */
const float Infinity = std::numeric_limits<float>::infinity();
const float PI = glm::pi<float>();
const float INV_PI = 1.0f / PI;

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
 * @brief Generates a random integer in range [min, max].
 * Note: This uses a closed interval (inclusive of both min and max).
 * 
 * @param min Minimum integer.
 * @param max Maximum integer.
 * @return int Random integer between min and max.
 */
inline int random_int(int min, int max) {
    static thread_local std::mt19937 generator;
    std::uniform_int_distribution<int> distribution(min, max);
    return distribution(generator);
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
 * @brief Generates a random vector inside a unit disk (with z-dim be zero).
 * Used for motion blurr.
 * 
 * @return glm::vec3 A vector with length < 1 and z = 0.
 */
inline glm::vec3 random_in_unit_disk() {
    while (true) {
        glm::vec3 p = glm::vec3(random_float(-1,1), random_float(-1,1), 0);
        if (glm::dot(p,p) >= 1) continue;
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
 * @brief Generate a random direction with Cosine-Weighted distribution.
 * Assumes the normal is (0, 0, 1).
 * Used for importance sampling Lambertian surfaces.
 * 
 * Z = sqrt(1-r2) implies the probability is proportional to Cos(theta).
 */
inline glm::vec3 random_cosine_direction() {
    float phi = random_float(0.0f, 2.0f * PI);
    // float r2 = random_float();
    float theta = random_float(0.0f, PI);
    float r2 = sin(theta) * sin(theta);
    float z = sqrt(1.0f - r2); 

    float x = cos(phi) * sqrt(r2);
    float y = sin(phi) * sqrt(r2);

    return glm::vec3(x, y, z);
}

/**
 * @brief Checks if a vector is near zero in all dimensions.
 * Used to prevent NaNs when a reflection vector is zero.
 */
inline bool near_zero(const glm::vec3& v) {
    return (std::fabs(v.x) < EPSILON) && (std::fabs(v.y) < EPSILON) && (std::fabs(v.z) < EPSILON);
}