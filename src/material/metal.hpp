#pragma once

#include "material_utils.hpp"
#include "../texture/solid_color.hpp"
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
    Metal(const glm::vec3& a, float f) : albedo(std::make_shared<SolidColor>(a)), fuzz(f < 1 ? f : 1) {}
    Metal(std::shared_ptr<Texture> a, float f) : albedo(a), fuzz(f < 1 ? f : 1) {}

    virtual bool scatter(const Ray& r_in, const HitRecord& rec, ScatterRecord& srec) const override {
        glm::vec3 reflected = glm::reflect(glm::normalize(r_in.direction()), rec.normal);
        
        // For simplicity, even if fuzz > 0, the distribution is continuous, we still view it as single-point distribution.
        // TODO: Microfacet GGX
        
        srec.is_specular = true; 
        srec.attenuation = albedo->value(rec.u, rec.v, rec.p);
        glm::vec3 scattered_dir = reflected + fuzz * random_in_unit_sphere();
        if (near_zero(scattered_dir))
            scattered_dir = reflected;
        srec.specular_ray = Ray(rec.p, scattered_dir, r_in.time());
        srec.pdf = 0.0f; // PDF of mirror reflection is meaningless.

        return (glm::dot(srec.specular_ray.direction(), rec.normal) > 0);
    }
    
    virtual bool is_emissive() const override { return false; }

public:
    std::shared_ptr<Texture> albedo;
    float fuzz;
};