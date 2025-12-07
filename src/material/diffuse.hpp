#pragma once

#include "material_utils.hpp"
#include "../core/onb.hpp"
#include "../texture/solid_color.hpp"
/**
 * @brief Lambertian (Diffuse) material.
 * Scatters rays randomly using a cosine-weighted distribution (approximated here).
 */
class Lambertian : public Material {
public:
    /**
     * @brief Construct using a solid color.
     */
    Lambertian(const glm::vec3& a) : albedo(std::make_shared<SolidColor>(a)) {}

    /**
     * @brief Construct using a texture.
     */
    Lambertian(std::shared_ptr<Texture> a) : albedo(a) {}
    

    /**
     * @brief Set a normal map texture.
     */
    void set_normal_map(std::shared_ptr<Texture> n_map) {
        normal_map = n_map;
    }


    virtual bool scatter(const Ray& r_in, const HitRecord& rec, ScatterRecord& srec) const override {
        srec.is_specular = false; // a diffuse event
        srec.attenuation = albedo->value(rec.u, rec.v, rec.p);

        glm::vec3 shading_normal = rec.normal;
        if (normal_map) {
            glm::vec3 map_val = normal_map->value(rec.u, rec.v, rec.p);
            // Convert [0, 1] color to [-1, 1] vector
            glm::vec3 local_n = 2.0f * map_val - glm::vec3(1.0f);
            
            // Create TBN basis from Geometry
            Onb tbn(rec.normal, rec.tangent);
            
            // Transform from Tangent Space to World Space
            shading_normal = glm::normalize(tbn.local(local_n));
            srec.shading_normal = shading_normal; 
        }

        // 1. Build Orthonormal Basis from surface normal
        Onb uvw(shading_normal);

        // 2. Sample a direction in Tangent Space (Cosine Weighted)
        // This vector is guaranteed to point outwards relative to the normal.
        glm::vec3 direction_local = random_cosine_direction();

        // 3. Transform to World Space
        glm::vec3 scatter_direction = uvw.local(direction_local);
        
        // 4. Create the ray
        srec.specular_ray = Ray(rec.p, glm::normalize(scatter_direction), r_in.time());

        // 5. Calculate PDF
        // For cosine-weighted sampling, p(direction) = cos(theta) / PI.
        // Note: The integrator uses this PDF to divide the result.
        // We use the same formula here so they cancel out nicely in the integrator logic.
        srec.pdf = glm::dot(shading_normal, glm::normalize(scatter_direction)) * INV_PI;

        return true;
    }

    /**
     * @brief Evaluates the Lambertian BRDF.
     * Formula: f_r = albedo / PI
     */
     virtual glm::vec3 eval(
        const Ray& r_in, const HitRecord& rec, const Ray& scattered, const glm::vec3& shading_normal
    ) const override {
        float cos_theta = glm::dot(shading_normal, glm::normalize(scattered.direction()));
        if (cos_theta <= 0) 
            return glm::vec3(0.0f);

        return albedo->value(rec.u, rec.v, rec.p) * INV_PI;
    }
    
    // Future-proofing: Lambertian PDF is cos(theta) / PI
    virtual float scattering_pdf(const Ray& r_in, const HitRecord& rec, const Ray& scattered, const glm::vec3& shading_normal) const override {
        float cosine = glm::dot(shading_normal, glm::normalize(scattered.direction()));
        return cosine < 0 ? 0 : cosine * INV_PI;
    }
    
    virtual bool is_emissive() const override { return false; }
    
    virtual bool is_specular() const override { return false; }

public:
    std::shared_ptr<Texture> albedo;
    std::shared_ptr<Texture> normal_map;
};