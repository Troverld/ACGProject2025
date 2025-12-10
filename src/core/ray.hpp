#pragma once
#include <glm/glm.hpp>

/**
 * @brief Represents a ray in 3D space defined by an origin and a direction.
 * Equation: P(t) = origin + t * direction
 * If constructed with a direction vector that is not normalized, it will be normalized internally.
 */
class Ray {
public:
    /**
     * @brief Default constructor.
     */
    Ray() {}

    /**
     * @brief Construct a new Ray object.
     * 
     * @param origin The starting point of the ray.
     * @param direction The direction vector of the ray (will be normalized).
     * @param time The time at which this ray exists (for motion blur).
     * @param wavelength The wavelength in nanometers (nm). 0.0f means full spectrum (white).
     */
    Ray(const glm::vec3& origin, const glm::vec3& direction, float time = 0.0f, float wavelength = 0.0f)
        : orig(origin), tm(time), wavelength(wavelength){
        dir = glm::normalize(direction);
        inv_dir = 1.0f / dir; 
    }

    glm::vec3 origin() const { return orig; }
    glm::vec3 direction() const { return dir; }
    glm::vec3 inv_direction() const { return inv_dir; }
    float time() const { return tm; }
    float get_wavelength() const { return wavelength; }

    /**
     * @brief Calculates the point along the ray at parameter t.
     * 
     * @param t The distance parameter.
     * @return glm::vec3 The 3D position P(t).
     */
    glm::vec3 at(float t) const {
        return orig + t * dir;
    }

public:
    glm::vec3 orig;
    glm::vec3 dir;
    glm::vec3 inv_dir; // Cached inverse direction
    float tm;
    float wavelength; // in nm, 0.0f means full spectrum (white)
};