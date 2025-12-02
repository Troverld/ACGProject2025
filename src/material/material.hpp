#pragma once

#include "../core/ray.hpp"
#include "../scene/object.hpp"
#include <glm/glm.hpp>

/**
 * @brief Class to record detailed information of a scatter.
 */
struct ScatterRecord {
    Ray specular_ray;       // The reflected ray if the surface is mirror.
    bool is_specular;       // Whether the surface is mirror.
    glm::vec3 attenuation;  // Albedo.
    float pdf;              // If the surface is not mirror, then the PDF of the sampled direction is recorded.
    glm::vec3 shading_normal;  // Perturbed normal from normal map (if any).
    ScatterRecord(glm::vec3 normal) : shading_normal(normal) {} // A normal initialization is a must. We should always have a valid normal.
};



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
     * @param srec The generated scatter record.
     * @return true If the ray is scattered.
     * @return false If the ray is absorbed (black body).
     */
    virtual bool scatter(
        const Ray& r_in, const HitRecord& rec, ScatterRecord& srec
    ) const = 0;


    /**
     * @brief Evaluate the BRDF (Bidirectional Reflectance Distribution Function).
     * Used for Direct Lighting (Next Event Estimation).
     * 
     * @param r_in The incoming ray from the camera/previous bounce.
     * @param rec The hit record containing normal and position.
     * @param scattered The outgoing ray towards the light source (for NEE).
     * @return glm::vec3 The BRDF value (color). 
     *         For Lambertian: albedo / PI. 
     *         For Specular/Mirror: 0.0 (Delta distributions cannot be evaluated explicitly).
     */
    virtual glm::vec3 eval(
        const Ray& r_in, const HitRecord& rec, const Ray& scattered, const glm::vec3& shading_normal
    ) const {
        return glm::vec3(0.0f, 0.0f, 0.0f);
    }
    
    /**
     * @brief Emitted light. 
     * Default is black (no emission). Override for Area Lights.
     */
    virtual glm::vec3 emitted(float u, float v, const glm::vec3& p) const {
        return glm::vec3(0.0f, 0.0f, 0.0f);
    }

    /**
     * @brief Probability Density Function value for a specific direction.
     * Essential for Importance Sampling (MIS).
     */
    virtual float scattering_pdf(const Ray& r_in, const HitRecord& rec, const Ray& scattered, const glm::vec3& shading_normal) const {
        return 0.0;
    }

    /**
     * @brief Check if this material emits light.
     * Used by Scene to automatically promote objects to Area Lights.
    */
    virtual bool is_emissive() const {
        return false;
    }
};