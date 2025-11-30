#pragma once

#include "../core/utils.hpp"
#include "../scene/object.hpp"
#include "../material/material.hpp" 
#include <glm/glm.hpp>
#include <memory>

/**
 * @brief Abstract base class for all lights (Point, Area, Directional).
 * Unlike Objects, Lights are sampled explicitly by the integrator.
 */
class Light {
public:
    virtual ~Light() = default;

    /**
     * @brief Sample the light source.
     * Given a point in the scene, this returns a direction towards the light,
     * the radiance arriving from that direction, and the PDF of choosing that direction.
     * 
     * @param origin The point being illuminated (e.g., surface intersection point).
     * @param wi Output: The direction vector from origin TO the light source.
     * @param pdf Output: The probability density of sampling this direction.
     * @param distance Output: Distance to the light point (for shadow ray t_max).
     * @return glm::vec3 The radiance (color * intensity) emitted.
     */
    virtual glm::vec3 sample_li(const glm::vec3& origin, glm::vec3& wi, float& pdf, float& distance) const = 0;
};

/**
 * @brief Area Light wrapper.
 * This bridges the gap between Geometry (Object) and Lighting (Light).
 * It holds a reference to a geometric object that has an emissive material.
 */
class DiffuseAreaLight : public Light {
public:
    /**
     * @param obj The geometric object that is emitting light.
     */
    DiffuseAreaLight(std::shared_ptr<Object> obj) : shape(obj) {}

    virtual glm::vec3 sample_li(const glm::vec3& origin, glm::vec3& wi, float& pdf, float& distance) const override {
        // 1. Ask the shape for a random direction towards it
        wi = shape->random_pointing_vector(origin);
        
        // 2. Calculate distance and normalize wi
        distance = glm::length(wi);
        wi = glm::normalize(wi);

        // 3. Get the PDF of this direction
        pdf = shape->pdf_value(origin, wi);
        if (pdf <= EPSILON) return glm::vec3(0.0f);

        // 4. Get the emitted radiance.
        // We assume the ray hits the "front" of the light.
        // In a robust system, we would raycast to find the specific UV, but here we simplify.
        // We evaluate emission at a dummy point because DiffuseLight usually emits uniformly.
        // Note: We multiply by Dot(N, -wi) inside the integrator or assume emission is Lambertian.
        
        // Retrieve emission from the material
        // We assume the shape has a material (Scene logic guarantees this).
        // Since we don't have the exact hit point on the light source here without tracing,
        // we assume uniform emission for area lights in this simplified implementation.
        return shape->get_material()->emitted(0, 0, origin + wi * distance); 
    }

public:
    std::shared_ptr<Object> shape;
};