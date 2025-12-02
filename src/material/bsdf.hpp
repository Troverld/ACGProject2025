#pragma once

#include "material.hpp"
#include "../core/utils.hpp"
#include "../core/onb.hpp"
#include "../texture/texture.hpp"
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

public:
    std::shared_ptr<Texture> albedo;
    std::shared_ptr<Texture> normal_map;
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

/**
 * @brief A material that emits light.
 * It does NOT scatter rays (it stops the bounce).
 */
class DiffuseLight : public Material {
public:
    DiffuseLight(const glm::vec3& c) : emit_texture(std::make_shared<SolidColor>(c)) {}
    DiffuseLight(std::shared_ptr<Texture> a) : emit_texture(a) {}

    virtual bool scatter(const Ray& r_in, const HitRecord& rec, ScatterRecord& srec)const override {
        return false; // No scattering, just emission (for now)
    }

    virtual glm::vec3 emitted(float u, float v, const glm::vec3& p) const override {
        return emit_texture->value(u, v, p);
    }
    
    virtual bool is_emissive() const override { return true; }

public:
    std::shared_ptr<Texture> emit_texture;
};

/**
 * @brief Dielectric material (Glass, Water, Diamond).
 * Handles both Reflection (BRDF) and Refraction (BTDF).
 */
class Dielectric : public Material {
public:
    /**
     * @param a The albedo (color) of the glass.
     * @param index_of_refraction Refractive index (e.g., Glass = 1.5, Water = 1.33, Diamond = 2.4).
     */
    Dielectric(const glm::vec3& a, float index_of_refraction) : albedo(a), ir(index_of_refraction) {}

    virtual bool scatter(const Ray& r_in, const HitRecord& rec, ScatterRecord& srec) const override {
        srec.is_specular = true;
        srec.pdf = 0.0f; // Delta attenuation
        srec.attenuation = albedo; // Clear glass implies no surface color absorption (usually).

        // 1. Determine refraction ratio (eta / eta_prime)
        // If front_face is true, ray is entering: Air (1.0) -> Glass (ir) => ratio = 1.0 / ir
        // If front_face is false, ray is exiting: Glass (ir) -> Air (1.0) => ratio = ir / 1.0
        float refraction_ratio = rec.front_face ? (1.0f / ir) : ir;

        glm::vec3 unit_direction = glm::normalize(r_in.direction());
        
        // 2. Calculate Cos theta and Sin theta
        // fmin ensures numerical stability (cos cannot exceed 1.0)
        float cos_theta = fmin(glm::dot(-unit_direction, rec.normal), 1.0f);
        float sin_theta = sqrt(fmax(0.0f, 1.0f - cos_theta * cos_theta));

        // 3. Total Internal Reflection (TIR) Check
        // If eta * sin_theta > 1.0, strictly reflect.
        bool cannot_refract = (refraction_ratio * sin_theta) > 1.0f;
        
        glm::vec3 direction;

        // 4. Determine Scatter Direction (Reflect vs Refract)
        // We use Schlick's approximation for Fresnel factor.
        if (cannot_refract || reflectance(cos_theta, refraction_ratio) > random_float()) {
            // Reflect (TIR or Fresnel Reflection)
            direction = glm::reflect(unit_direction, rec.normal);
        } else {
            // Refract (Snell's Law)
            direction = glm::refract(unit_direction, rec.normal, refraction_ratio);
        }
        
        srec.specular_ray = Ray(rec.p, direction, r_in.time());
        return true;
    }

    virtual bool is_emissive() const override { return false; }

private:
    /**
     * @brief Schlick's approximation for reflectance.
     * Approximates the contribution of Fresnel factor (how much light reflects vs refracts)
     * based on the viewing angle.
     */
    static float reflectance(float cosine, float ref_idx) {
        // R0 = ((n1 - n2) / (n1 + n2))^2
        auto r0 = (1.0f - ref_idx) / (1.0f + ref_idx);
        r0 = r0 * r0;
        return r0 + (1.0f - r0) * pow((1.0f - cosine), 5.0f);
    }

public:
    glm::vec3 albedo;
    float ir; // Index of Refraction
};