#pragma once

#include "material_utils.hpp"
#include "../texture/solid_color.hpp"
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

    virtual bool is_specular() const override { return true; }

private:
    /**
     * @brief Schlick's approximation for reflectance.
     * Approximates the contribution of Fresnel factor (how much light reflects vs refracts)
     * based on the viewing angle.
     */
    static float reflectance(float cosine, float ref_idx) {
        // R0 = ((n1 - n2) / (n1 + n2))^2
        float r0 = (1.0f - ref_idx) / (1.0f + ref_idx);
        r0 = r0 * r0;
        return r0 + (1.0f - r0) * pow((1.0f - cosine), 5.0f);
    }

public:
    glm::vec3 albedo;
    float ir; // Index of Refraction
};