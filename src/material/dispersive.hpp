#pragma once

#include "material_utils.hpp"

/**
 * @brief A dielectric material that exhibits chromatic dispersion.
 * Uses Stochastic Spectral Sampling: incoming white rays are randomly assigned 
 * a wavelength, and their IOR is calculated using Cauchy's Equation.
 */
class DispersiveGlass : public Material {
public:
    /**
     * @brief Construct a new Dispersive Glass.
     * 
     * @param a The base color (albedo), usually white for glass.
     * @param cauchy_a The A coefficient in Cauchy's equation (approx. Refractive Index).
     * @param cauchy_b The B coefficient in Cauchy's equation (Dispersion strength).
     *                 For Borosilicate glass (BK7), A ~ 1.5046, B ~ 0.0042 (with lambda in micrometers).
     */
    DispersiveGlass(const glm::vec3& a, float cauchy_a, float cauchy_b) 
        : albedo(a), A(cauchy_a), B(cauchy_b) {}

    virtual bool scatter(const Ray& r_in, const HitRecord& rec, ScatterRecord& srec) const override {
        srec.is_specular = true;
        srec.pdf = 0.0f; // Delta distribution

        float lambda_nm;
        glm::vec3 color_filter;

        // 1. Stochastic Wavelength Sampling
        // If the ray is "White" (0.0), assign a random wavelength in visible spectrum.
        if (r_in.get_wavelength() <= EPSILON) {
            // Sample range [380, 780] nm
            lambda_nm = random_float(380.0f, 780.0f);
            
            // Convert wavelength to RGB to simulate the color of this specific light ray
            // We multiply by albedo to allow tinted glass.
            color_filter = wavelength_to_rgb(lambda_nm) * albedo * 3.0f; // Scale to maintain brightness
        } else {
            // Ray is already monochromatic, preserve its wavelength
            lambda_nm = r_in.get_wavelength();
            
            // Since the ray is already colored from the previous bounce,
            // we pass 1.0 (white) as attenuation so we don't darken it further unnecessarily,
            // or we could apply the albedo again if it's absorptive.
            // For simple clear glass, we pass white.
            color_filter = glm::vec3(1.0f);
        }

        srec.attenuation = color_filter;

        // 2. Calculate IOR using Cauchy's Equation
        // n(lambda) = A + B / lambda^2
        // Note: Standard Cauchy B implies lambda is in micrometers.
        float lambda_um = lambda_nm / 1000.0f;
        float refraction_index = A + B / (lambda_um * lambda_um);

        // 3. Standard Dielectric Logic (Reflect/Refract) using the specific IOR
        float refraction_ratio = rec.front_face ? (1.0f / refraction_index) : refraction_index;

        glm::vec3 unit_direction = glm::normalize(r_in.direction());
        
        float cos_theta = fmin(glm::dot(-unit_direction, rec.normal), 1.0f);
        float sin_theta = sqrt(fmax(0.0f, 1.0f - cos_theta * cos_theta));

        bool cannot_refract = (refraction_ratio * sin_theta) > 1.0f;
        
        glm::vec3 direction;
        if (cannot_refract || reflectance(cos_theta, refraction_ratio) > random_float()) {
            direction = glm::reflect(unit_direction, rec.normal);
        } else {
            direction = glm::refract(unit_direction, rec.normal, refraction_ratio);
        }
        
        // 4. Generate scattered ray CARRYING the wavelength info
        srec.specular_ray = Ray(rec.p, direction, r_in.time(), lambda_nm);

        return true;
    }

    virtual bool is_specular() const override { return true; }
    virtual bool is_emissive() const override { return false; }
    virtual bool is_transparent() const override { return true; }

    virtual glm::vec3 evaluate_transmission(const HitRecord& rec) const override {
        // Dispersive glass usually has an albedo member already.
        // Just return it.
        return albedo;
    }

private:
    static float reflectance(float cosine, float ref_idx) {
        float r0 = (1.0f - ref_idx) / (1.0f + ref_idx);
        r0 = r0 * r0;
        return r0 + (1.0f - r0) * pow((1.0f - cosine), 5.0f);
    }

public:
    glm::vec3 albedo;
    float A; // Cauchy A (Base Index)
    float B; // Cauchy B (Dispersion Coefficient)
};