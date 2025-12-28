#pragma once

#include "material_utils.hpp"
#include "../texture/solid_color.hpp"
#include "../core/onb.hpp"

/**
 * @brief Isotropic Material (Phase Function).
 * Represents a volume medium where light scatters uniformly in all directions.
 * Used for fog, smoke, etc.
 */
class Isotropic : public Material {
public:
    /**
     * @brief Construct with a solid color (non-emissive).
     */
    Isotropic(glm::vec3 c)
        : albedo(std::make_shared<SolidColor>(c)), emit(std::make_shared<SolidColor>(0.0f, 0.0f, 0.0f)), emissive(false) {}

    /**
     * @brief Construct with a texture (non-emissive).
     */
    Isotropic(std::shared_ptr<Texture> a)
        : albedo(a), emit(std::make_shared<SolidColor>(0.0f, 0.0f, 0.0f)), emissive(false) {}

    /**
     * @brief Construct with Albedo and Emission (Texture).
     */
    Isotropic(std::shared_ptr<Texture> a, std::shared_ptr<Texture> e) 
        : albedo(a), emit(e), emissive(true) {}
    
    /**
     * @brief Construct with Albedo and Emission (Color).
     */
    Isotropic(glm::vec3 a, glm::vec3 e) 
        : albedo(std::make_shared<SolidColor>(a)), emit(std::make_shared<SolidColor>(e)), emissive(true) {}

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
        
        srec.specular_ray = Ray(rec.p, scattered_dir, r_in.time(), r_in.get_wavelength());
        
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

    /**
     * @brief Handle Volumetric Emission.
     * However, promoting a Volume to a Light source is very complex (requires sampling the volume volume).
     * For now, we rely on implicit path tracing (rays randomly hitting the smoke) to pick up the light.
     * So we can return true here, but be aware standard NEE won't target the volume unless you implement Light::sample_li for ConstantMedium.
     */
    virtual glm::vec3 emitted(float u, float v, const glm::vec3& p) const override {
        return emit->value(u, v, p);
    }
    
    /**
     * @brief Check if volume is emissive.
     * Note: Current Scene::add logic only promotes Objects to Lights if is_emissive is true.
     */
    virtual bool is_emissive() const override { 
        return emissive;
    }
    
    // TODO: specular volumetric
    virtual bool is_specular() const override { return false; }

public:
    std::shared_ptr<Texture> albedo;
    std::shared_ptr<Texture> emit;
    bool emissive;
};