#pragma once

#include "material.hpp"
#include "../core/utils.hpp"

/**
 * @brief Lambertian (Diffuse) material.
 * Scatters rays randomly using a cosine-weighted distribution (approximated here).
 */
class Lambertian : public Material {
public:
    /**
     * @param a The albedo (color) of the material.
     */
    Lambertian(const glm::vec3& a) : albedo(a) {}

    virtual bool scatter(
        const Ray& r_in, const HitRecord& rec, glm::vec3& attenuation, Ray& scattered
    ) const override {
        // Generate a random direction for diffuse bounce
        glm::vec3 scatter_direction = rec.normal + random_unit_vector();

        // Catch degenerate scatter direction (if random vector is exactly opposite to normal)
        if (near_zero(scatter_direction))
            scatter_direction = rec.normal;

        scattered = Ray(rec.p, scatter_direction);
        attenuation = albedo;
        return true;
    }

public:
    glm::vec3 albedo;
};

/**
 * @brief Metal (Specular) material.
 * Reflects rays perfectly or with some fuzziness.
 */
class Metal : public Material {
public:
    /**
     * @param a The albedo (color) of the reflection.
     * @param f Fuzziness factor [0, 1]. 0 is perfect mirror.
     */
    Metal(const glm::vec3& a, float f) : albedo(a), fuzz(f < 1 ? f : 1) {}

    virtual bool scatter(
        const Ray& r_in, const HitRecord& rec, glm::vec3& attenuation, Ray& scattered
    ) const override {
        glm::vec3 reflected = glm::reflect(glm::normalize(r_in.direction()), rec.normal);
        
        // Add fuzziness by perturbing the reflected ray
        scattered = Ray(rec.p, reflected + fuzz * random_in_unit_sphere());
        attenuation = albedo;
        
        // Only scatter if the ray is in the same hemisphere as the normal
        return (glm::dot(scattered.direction(), rec.normal) > 0);
    }

public:
    glm::vec3 albedo;
    float fuzz;
};