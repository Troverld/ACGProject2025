#pragma once

#include "../scene/scene.hpp"
#include "../scene/camera.hpp"
#include "../material/material_utils.hpp"
#include "../core/distribution.hpp"
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
    std::unique_ptr<Distribution1D> light_distribution;

    /**
     * @brief Build the light distribution based on power.
     * Is automatically integrated into construction of the integrator.
     */
    void preprocess(const Scene& scene) {
        std::vector<float> powers;
        for (const auto& light : scene.lights) {
            powers.push_back(light->power());
        }
        if (scene.env_light && scene.env_light->power() > EPSILON) {
            powers.push_back(scene.env_light->power());
        }

        if (!powers.empty()) {
            light_distribution = std::make_unique<Distribution1D>(powers.data(), powers.size());
        }
    }

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
    glm::vec3 sample_one_light(const Scene& scene, const HitRecord& rec, const ScatterRecord& srec, const Ray& current_ray, const bool local_light_caustic) const {
        if (!light_distribution || light_distribution->count() == 0) return glm::vec3(0.0f);
        // 1. Sample a light source based on its power
        float light_select_pdf;
        float u_remap; // Remapped random number (unused here but required by signature)
        int light_idx = light_distribution->sample_discrete(random_float(), light_select_pdf, u_remap);

        const Light* light;
        size_t n_scene_lights = scene.lights.size();
        bool caustic = local_light_caustic;

        // Determine which light was picked (Scene Lights vs Env Light)
        if (light_idx < static_cast<int>(n_scene_lights)) {
            light = scene.lights[light_idx].get();
        } else {
            light = scene.env_light.get();
            caustic = true;
        }

        glm::vec3 to_light;
        float light_pdf; // PDF of sampling this point on light
        float dist;
        
        glm::vec3 L_emitted = light->sample_li(rec.p, to_light, light_pdf, dist);

        if (light_pdf <= EPSILON || near_zero(L_emitted)) return glm::vec3(0.0f);

        Ray shadow_ray(rec.p, to_light, current_ray.time());
        
        glm::vec3 f_r = rec.mat_ptr->eval(current_ray, rec, shadow_ray, srec.shading_normal);
        
        if (near_zero(f_r)) return glm::vec3(0.0f);
        
        float cos_theta = glm::dot(srec.shading_normal, glm::normalize(to_light));
        
        if (cos_theta <= 0.0f) return glm::vec3(0.0f);
        
        HitRecord shadow_rec;
        glm::vec3 visibility(1.0f);
        visibility = scene.transmittance(shadow_ray, dist - SHADOW_EPSILON, 5, caustic);
        if (near_zero(visibility)) return glm::vec3(0.0f); // In shadow

        // Calculate BSDF PDF for this NEE direction
        float bsdf_pdf = rec.mat_ptr->scattering_pdf(current_ray, rec, shadow_ray, srec.shading_normal);
        
        float total_light_pdf = light_select_pdf * light_pdf;

        float weight = power_heuristic(total_light_pdf, bsdf_pdf);

        return L_emitted * f_r * cos_theta * weight * visibility / total_light_pdf;
    }
    
    /**
     * @brief Handle ray missing geometry (Environment lookup + MIS).
     */
    glm::vec3 eval_environment(const Scene& scene, const Ray& r, float bsdf_pdf, bool is_specular) const {
        glm::vec3 env_color = scene.sample_background(r);
        
        // If pure specular bounce or no lights, take full contribution
        if (is_specular || !light_distribution || !scene.env_light) {
            return env_color;
        }

        // MIS: Calculate the probability that we would have picked the EnvLight via sample_one_light
        // The EnvLight index is always at the end of the distribution array
        int env_idx = static_cast<int>(scene.lights.size());
        float light_select_pdf = light_distribution->pdf_discrete(env_idx);
        
        // PDF of sampling this direction on the EnvLight (handled by envirlight.hpp logic)
        // Note: For infinite lights, pdf_value returns solid angle PDF.
        float light_dir_pdf = scene.env_light->pdf_value(glm::vec3(0), r.direction());
        
        float total_light_pdf = light_select_pdf * light_dir_pdf;

        float weight = power_heuristic(bsdf_pdf, total_light_pdf);
        return env_color * weight;
    }

    /**
     * @brief Handle ray hitting a light source directly (Emission + MIS).
     */
    glm::vec3 eval_emission(const Scene& scene, const HitRecord& rec, const Ray& r, float bsdf_pdf, bool is_specular) const {
        glm::vec3 emitted = rec.mat_ptr->emitted(rec.u, rec.v, rec.p);
        
        // If pure specular or no light sampling setup, return full emission
        if (is_specular || !light_distribution) {
            return emitted;
        }

        // MIS: We hit a light geometry via BSDF. We need to find P(picking this light via NEE).
        // This requires finding the index of the light in scene.lights that corresponds to the hit object.
        int light_idx = rec.object->get_light_id();

        // If light_idx is -1, it means this object emits light but wasn't registered 
        // in the scene.lights list (shouldn't happen with correct Scene::add logic), 
        // or we hit the back of a single-sided light.
        if (light_idx < 0) return emitted;

        float light_select_pdf = light_distribution->pdf_discrete(light_idx);
        float area_pdf = rec.object->pdf_value(r.origin(), r.direction()); // Solid Angle PDF
        float total_light_pdf = light_select_pdf * area_pdf;

        float weight = power_heuristic(bsdf_pdf, total_light_pdf);
        return emitted * weight;
    }
};
