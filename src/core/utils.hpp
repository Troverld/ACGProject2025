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

// Padding for bounding box
const float PADDING_EPSILON = 1e-3f;

/**
 * @brief Generates a random float in range [0.0, 1.0).
 * 
 * @return float Random value between 0.0 (inclusive) and 1.0 (exclusive).
 */
inline float random_float() {
    static thread_local std::mt19937 generator(std::random_device{}()); 
    static std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
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
    static thread_local std::mt19937 generator(std::random_device{}());
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
 * Corrected implementation using concentric disk mapping or valid math.
 */
inline glm::vec3 random_cosine_direction() {
    float r1 = random_float();
    float r2 = random_float();

    float phi = 2.0f * PI * r1;
    
    // x, y on the disk, z defined by sphere constraint
    float x = cos(phi) * sqrt(r2);
    float y = sin(phi) * sqrt(r2);
    float z = sqrt(1.0f - r2); 

    return glm::vec3(x, y, z);
}

/**
 * @brief Checks if a vector is near zero in all dimensions.
 * Used to prevent NaNs when a reflection vector is zero.
 */
inline bool near_zero(const glm::vec3& v) {
    return (std::fabs(v.x) < EPSILON) && (std::fabs(v.y) < EPSILON) && (std::fabs(v.z) < EPSILON);
}

/**
 * @brief Utility to map a point on a unit sphere to [0,1] UV coordinates.
 * Used by Sphere objects and Spherical Environment Maps.
 * 
 * @param p Point on the unit sphere (must be normalized).
 * @param u Output [0, 1] horizontal coordinate.
 * @param v Output [0, 1] vertical coordinate.
 */
inline void get_sphere_uv(const glm::vec3& p, float& u, float& v) {
    // p: a given point on the sphere of radius one, centered at the origin.
    // u: returned value [0,1] of angle around the Y axis from X=-1.
    // v: returned value [0,1] of angle from Y=-1 to Y=+1.
    
    float theta = acos(-p.y);
    float phi = atan2(-p.z, p.x) + PI;

    u = phi / (2 * PI);
    v = theta / PI;
}

/**
 * @brief Inverse of get_sphere_uv. Maps UV [0,1] back to a unit vector on the sphere.
 * Ensures strict consistency with the get_sphere_uv coordinate system.
 * 
 * @param u Horizontal coordinate [0, 1]
 * @param v Vertical coordinate [0, 1]
 * @return glm::vec3 Normalized direction vector
 */
inline glm::vec3 uv_to_sphere(float u, float v) {
    float theta = v * PI;
    float phi = u * 2.0f * PI;

    float sin_theta = std::sin(theta);
    float cos_theta = std::cos(theta);
    float sin_phi = std::sin(phi);
    float cos_phi = std::cos(phi);

    // Derived from get_sphere_uv logic:
    // y = -cos(theta)
    // x = -sin(theta) * cos(phi)
    // z = sin(theta) * sin(phi)
    
    return glm::vec3(-sin_theta * cos_phi, -cos_theta, sin_theta * sin_phi);
}

inline float grayscale(const glm::vec3& color) {
    return 0.2126f * color.r + 0.7152f * color.g + 0.0722f * color.b;
}


/**
 * @brief Approximates RGB values from a wavelength in nanometers.
 * Based on the "Spectra" approximation (Dan Bruton).
 * Covers the visible spectrum approx 380nm to 780nm.
 * 
 * @param lambda Wavelength in nanometers.
 * @return glm::vec3 Normalized RGB color.
 */
inline glm::vec3 wavelength_to_rgb(float lambda) {
    float r, g, b;
    if (lambda >= 380.0f && lambda < 440.0f) {
        r = -(lambda - 440.0f) / (440.0f - 380.0f);
        g = 0.0f;
        b = 1.0f;
    } else if (lambda >= 440.0f && lambda < 490.0f) {
        r = 0.0f;
        g = (lambda - 440.0f) / (490.0f - 440.0f);
        b = 1.0f;
    } else if (lambda >= 490.0f && lambda < 510.0f) {
        r = 0.0f;
        g = 1.0f;
        b = -(lambda - 510.0f) / (510.0f - 490.0f);
    } else if (lambda >= 510.0f && lambda < 580.0f) {
        r = (lambda - 510.0f) / (580.0f - 510.0f);
        g = 1.0f;
        b = 0.0f;
    } else if (lambda >= 580.0f && lambda < 645.0f) {
        r = 1.0f;
        g = -(lambda - 645.0f) / (645.0f - 580.0f);
        b = 0.0f;
    } else if (lambda >= 645.0f && lambda <= 780.0f) {
        r = 1.0f;
        g = 0.0f;
        b = 0.0f;
    } else {
        return glm::vec3(0.0f);
    }

    // Let the intensity fall off near the vision limits
    float factor;
    if (lambda >= 380.0f && lambda < 420.0f) {
        factor = 0.3f + 0.7f * (lambda - 380.0f) / (420.0f - 380.0f);
    } else if (lambda >= 420.0f && lambda < 700.0f) {
        factor = 1.0f;
    } else if (lambda >= 700.0f && lambda <= 780.0f) {
        factor = 0.3f + 0.7f * (780.0f - lambda) / (780.0f - 700.0f);
    } else {
        factor = 0.0f;
    }

    // Gamma correct (approximate)
    // Note: If we are working in linear space, we might not need pow(0.8),
    // but this function is often used to map to displayable colors.
    // Keeping it linear here for the renderer accumulator.
    return glm::vec3(r * factor, g * factor, b * factor);
}