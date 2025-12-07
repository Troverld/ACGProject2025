#pragma once

#include "../scene/scene.hpp"
#include "../scene/camera.hpp"
#include "../material/material_utils.hpp"
#include "../core/utils.hpp"
#include <glm/glm.hpp>

/**
 * @brief Abstract base class for rendering algorithms.
 */
class Integrator {
public:
    virtual ~Integrator() = default;
    
    /**
     * @brief Calculates the radiance for a given ray.
     */
    virtual glm::vec3 estimate_radiance(const Ray& r, const Scene& scene) const = 0;

protected:

    /**
     * @brief Utility for Power Heuristic.
     * Balance Heuristic: f^1 / (f^1 + g^1)
     * Power Heuristic:   f^2 / (f^2 + g^2) (Usually works better)
     */
    float power_heuristic(float pdf_f, float pdf_g) const {
        float f2 = pdf_f * pdf_f;
        float g2 = pdf_g * pdf_g;
        float denom = f2 + g2;
        return denom > 0.0f ? f2 / denom : 0.0f;
    }
    /**
     * @brief Clamps the radiance to avoid fireflies.
     * @param L The radiance to clamp.
     */
    void clamp_radiance(glm::vec3 &L, float limit = 5.0f) const {
        float lum = glm::length(L);
        if (lum > limit) {
            L = L * (limit / lum);
        }
        return;
    }

    /**
     * @brief Samples a random light source for direct lighting (Next Event Estimation).
     * Exposed for use in PhotonIntegrator.
     * 
     * @param scene The scene reference.
     * @param rec The hit record of the current surface point.
     * @param srec The scatter record (contains material info).
     * @param time The time of the ray (for motion blur).
     * @return glm::vec3 The UNWEIGHTED direct radiance (not multiplied by path throughput yet).
     */
    glm::vec3 sample_one_light(const Scene& scene, const HitRecord& rec, const ScatterRecord& srec, const Ray& current_ray) const {
        size_t n_lights = scene.lights.size() + (scene.env_light ? 1 : 0);
        if (n_lights == 0) return glm::vec3(0.0f);
        int light_idx = random_int(0, static_cast<int>(n_lights) - 1);
        const auto& light = ((light_idx == scene.lights.size()) ? scene.env_light: scene.lights[light_idx]);
        float light_weight = static_cast<float>(n_lights); // 1 / (1/N)

        glm::vec3 to_light;
        float light_pdf; // PDF of sampling this point on light
        float dist;
        
        glm::vec3 L_emitted = light->sample_li(rec.p, to_light, light_pdf, dist);

        if (light_pdf <= EPSILON || near_zero(L_emitted)) return glm::vec3(0.0f);

        Ray shadow_ray(rec.p, to_light, current_ray.time());
        HitRecord shadow_rec;
        
        if (scene.intersect(shadow_ray, SHADOW_EPSILON, dist - SHADOW_EPSILON, shadow_rec)) return glm::vec3(0.0f); // In shadow

        glm::vec3 f_r = rec.mat_ptr->eval(current_ray, rec, shadow_ray, srec.shading_normal);
        
        if (near_zero(f_r)) return glm::vec3(0.0f);

        float cos_theta = glm::dot(srec.shading_normal, glm::normalize(to_light));

        if (cos_theta <= 0.0f) return glm::vec3(0.0f);

        // Calculate BSDF PDF for this NEE direction
        float bsdf_pdf = rec.mat_ptr->scattering_pdf(current_ray, rec, shadow_ray, srec.shading_normal);
        
        // MIS Weight: balance Light sampling vs BSDF sampling
        // We selected a specific light (prob 1/N), then a point on it (light_pdf).
        // Total light technique PDF = (1/N) * light_pdf
        float total_light_pdf = light_pdf / light_weight; 

        float weight = power_heuristic(total_light_pdf, bsdf_pdf);

        return L_emitted * f_r * cos_theta * weight / total_light_pdf;
    }
};
