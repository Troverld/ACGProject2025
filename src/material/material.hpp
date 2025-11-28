#pragma once

#include "../core/ray.hpp"
#include "../scene/object.hpp"
#include <glm/glm.hpp>

/**
 * @brief Abstract base class for materials.
 * Describes how a ray interacts with a surface.
 */
class Material {
public:
    virtual ~Material() = default;

    /**
     * @brief Scatter function to determine how a ray reflects/refracts off a surface.
     * 
     * @param r_in The incoming ray.
     * @param rec The hit record containing geometric info (normal, p, etc.).
     * @param attenuation Output color attenuation (albedo).
     * @param scattered Output scattered ray.
     * @return true If the ray is scattered.
     * @return false If the ray is absorbed (black body).
     */
    virtual bool scatter(
        const Ray& r_in, const HitRecord& rec, glm::vec3& attenuation, Ray& scattered
    ) const = 0;
};