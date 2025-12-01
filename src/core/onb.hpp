#pragma once
#include <glm/glm.hpp>

/**
 * @brief Orthonormal Basis (ONB) helper class.
 * Used to transform vectors between World Space and Local (Tangent) Space.
 * In Local Space, the surface normal is always (0, 0, 1).
 */
class Onb {
public:
    Onb() {}

    /**
     * @brief Construct an ONB from a surface normal (w).
     * Calculates u and v such that {u, v, w} form a right-handed orthonormal basis.
     */
    void build_from_w(const glm::vec3& n) {
        axis[2] = glm::normalize(n);
        
        // Check if n is parallel to the world Y axis to avoid singularity
        glm::vec3 a = (fabs(w().x) > 0.9f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
        
        axis[1] = glm::normalize(glm::cross(w(), a));
        axis[0] = glm::cross(w(), v());
    }

    const glm::vec3& u() const { return axis[0]; }
    const glm::vec3& v() const { return axis[1]; }
    const glm::vec3& w() const { return axis[2]; }

    /**
     * @brief Transform a vector from Local Space (Tangent Space) to World Space.
     */
    glm::vec3 local(float a, float b, float c) const {
        return a * u() + b * v() + c * w();
    }

    /**
     * @brief Transform a vector from Local Space (Tangent Space) to World Space.
     */
    glm::vec3 local(const glm::vec3& a) const {
        return a.x * u() + a.y * v() + a.z * w();
    }
    
    /**
     * @brief Transform a vector from World Space to Local Space.
     * Useful for evaluating BRDFs which are defined in local terms.
     */
    glm::vec3 world_to_local(const glm::vec3& a) const {
        return glm::vec3(glm::dot(a, u()), glm::dot(a, v()), glm::dot(a, w()));
    }

public:
    glm::vec3 axis[3];
};