#pragma once

#include "../core/ray.hpp"
#include <glm/glm.hpp>
#include "../core/utils.hpp"
#include "../core/record.hpp"



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
    
    /**
     * @brief Check if this material causes caustics (Glass, Mirror, etc.)
     * Used by PhotonIntegrator to find emission targets.
     */
    virtual bool is_specular() const {
        return false;
    }

    /**
     * @brief Check if the material allows light to pass through (Shadow Ray Transparency).
     * Used for Dielectric / DispersiveGlass to cast colored/faint shadows.
     * Note: Metal is specular but NOT transparent.
     */
    virtual bool is_transparent() const {
        return false;
    }

    /**
     * @brief Defines the color/intensity of light that passes through the material
     * during a shadow ray check (Fake Caustics / Colored Shadows).
     * 
     * @param rec The hit record (allows texture sampling based on UV).
     * @return glm::vec3 The attenuation factor (e.g., vec3(0.9) or vec3(1, 0, 0)).
     */
    virtual glm::vec3 evaluate_transmission(const HitRecord& rec) const {
        // Default behavior for opaque objects: block all light.
        // Although is_transparent() usually guards this call, returning 0 is a safe default.
        return glm::vec3(0.0f);
    }
};