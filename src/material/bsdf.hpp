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

    virtual bool scatter(const Ray& r_in, const HitRecord& rec, ScatterRecord& srec) const override {
        srec.is_specular = false; // a diffuse event
        srec.attenuation = albedo;

        glm::vec3 scatter_direction = rec.normal + random_unit_vector();
        if (near_zero(scatter_direction)) scatter_direction = rec.normal;
        
        srec.specular_ray = Ray(rec.p, scatter_direction, r_in.time());
        
        srec.pdf = this->scattering_pdf(r_in, rec, srec.specular_ray);
        
        return true;
    }

    /**
     * @brief Evaluates the Lambertian BRDF.
     * Formula: f_r = albedo / PI
     */
    virtual glm::vec3 eval(
        const Ray& r_in, const HitRecord& rec, const Ray& scattered
    ) const override {
        // Dot product check: strictly speaking, light must come from outside.
        // But the integrator handles the cosine term. We just return the material property.
        // Check if the scattered ray is on the same side as the normal
        float cos_theta = glm::dot(rec.normal, glm::normalize(scattered.direction()));
        if (cos_theta <= 0) 
            return glm::vec3(0.0f);

        return albedo / PI;
    }
    
    // Future-proofing: Lambertian PDF is cos(theta) / PI
    virtual float scattering_pdf(const Ray& r_in, const HitRecord& rec, const Ray& scattered) const override {
        float cosine = glm::dot(rec.normal, glm::normalize(scattered.direction()));
        return cosine < 0 ? 0 : cosine / PI;
    }
    
    virtual bool is_emissive() const override { return false; }

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

    virtual bool scatter(const Ray& r_in, const HitRecord& rec, ScatterRecord& srec) const override {
        glm::vec3 reflected = glm::reflect(glm::normalize(r_in.direction()), rec.normal);
        
        // For simplicity, even if fuzz > 0, the distribution is continuous, we still view it as single-point distribution.
        
        srec.is_specular = true; 
        srec.attenuation = albedo;
        srec.specular_ray = Ray(rec.p, reflected + fuzz * random_in_unit_sphere(), r_in.time());
        srec.pdf = 0.0f; // PDF of mirror reflection is meaningless.

        return (glm::dot(srec.specular_ray.direction(), rec.normal) > 0);
    }
    
    virtual bool is_emissive() const override { return false; }

public:
    glm::vec3 albedo;
    float fuzz;
};

/**
 * @brief A material that emits light.
 * It does NOT scatter rays (it stops the bounce).
 */
class DiffuseLight : public Material {
public:
    DiffuseLight(const glm::vec3& c) : emit_color(c) {}

    virtual bool scatter(const Ray& r_in, const HitRecord& rec, ScatterRecord& srec)const override {
        return false; // No scattering, just emission (for now)
    }

    virtual glm::vec3 emitted(float u, float v, const glm::vec3& p) const override {
        return emit_color;
    }
    
    virtual bool is_emissive() const override { return true; }

public:
    glm::vec3 emit_color;
};