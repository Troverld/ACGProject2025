#pragma once
#include <glm/glm.hpp>

/**
 * @brief Represents a ray in 3D space defined by an origin and a direction. If constructed with a direction vector that is not normalized, it will be normalized internally.
 * Equation: P(t) = origin + t * direction
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
     */
    Ray(const glm::vec3& origin, const glm::vec3& direction)
        : orig(origin), dir(glm::normalize(direction)) 
    {}

    /**
     * @return glm::vec3 The origin of the ray.
     */
    glm::vec3 origin() const { return orig; }

    /**
     * @return glm::vec3 The normalized direction of the ray.
     */
    glm::vec3 direction() const { return dir; }

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
};