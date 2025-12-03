#pragma once

#include "material.hpp"
#include "../texture/solid_color.hpp"
#include "../core/utils.hpp"
#include "../core/onb.hpp"

/**
 * @brief Isotropic Material (Phase Function).
 * Represents a volume medium where light scatters uniformly in all directions.
 * Used for fog, smoke, etc.
 */
class Isotropic : public Material {
public:
    /**
     * @brief Construct with a solid color.
     */
    Isotropic(glm::vec3 c) : albedo(std::make_shared<SolidColor>(c)) {}

    /**
     * @brief Construct with a texture.
     */
    Isotropic(std::shared_ptr<Texture> a) : albedo(a) {}

    /**
     * @brief Scatters light uniformly in a random direction.
     * Unlike surface reflection, the normal doesn't constrain the hemisphere; 
     * the ray can scatter anywhere on the sphere (4pi steradians).
     */
    virtual bool scatter(
        const Ray& r_in, const HitRecord& rec, ScatterRecord& srec
    ) const override {
        srec.is_specular = false; // It's diffuse-like (actually volumetric)
        srec.attenuation = albedo->value(rec.u, rec.v, rec.p);
        
        // Scatter inside the volume: Pick a random point on unit sphere (Directional independent)
        glm::vec3 scattered_dir = random_unit_vector();
        
        srec.specular_ray = Ray(rec.p, scattered_dir, r_in.time());
        
        // PDF for uniform sphere sampling is 1 / (4 * PI)
        srec.pdf = 1.0f / (4.0f * PI); 
        
        return true;
    }

    /**
     * @brief Evaluate PDF.
     * Uniform sphere distribution.
     */
    virtual float scattering_pdf(
        const Ray& r_in, const HitRecord& rec, const Ray& scattered, const glm::vec3& shading_normal
    ) const override {
        return 1.0f / (4.0f * PI);
    }
    
    // Volumetric scattering usually doesn't define an explicit eval like BRDF 
    // because it depends on phase function integral. 
    // For basic path tracing loop matching, we return albedo * phase_value.
    virtual glm::vec3 eval(
        const Ray& r_in, const HitRecord& rec, const Ray& scattered, const glm::vec3& shading_normal
    ) const override {
         return albedo->value(rec.u, rec.v, rec.p) * (1.0f / (4.0f * PI));
    }

public:
    std::shared_ptr<Texture> albedo;
};