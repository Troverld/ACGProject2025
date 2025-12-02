#pragma once

#include "../scene/scene.hpp"
#include "../scene/camera.hpp"
#include "../material/material.hpp"
#include "../core/utils.hpp"
#include <glm/glm.hpp>

/**
 * @brief Utility for Power Heuristic.
 * Balance Heuristic: f^1 / (f^1 + g^1)
 * Power Heuristic:   f^2 / (f^2 + g^2) (Usually works better)
 */
inline float power_heuristic(float pdf_f, float pdf_g) {
    float f2 = pdf_f * pdf_f;
    float g2 = pdf_g * pdf_g;
    return f2 / (f2 + g2);
}

/**
 * @brief Abstract base class for rendering algorithms.
 */
class Integrator {
public:
    virtual ~Integrator() = default;
    
    /**
     * @brief Calculates the radiance for a given ray.
     */
    virtual glm::vec3 estimate_radiance(const Ray& r, const Scene& scene, int depth) const = 0;
};

/**
 * @brief Path Tracer with Multiple Importance Sampling (MIS).
 */
class PathIntegrator : public Integrator {
public:
    PathIntegrator(int max_d) : max_depth(max_d) {}

    glm::vec3 estimate_radiance(const Ray& start_ray, const Scene& scene, int depth) const {
        Ray current_ray = start_ray;
        glm::vec3 L(0.0f);           
        glm::vec3 throughput(1.0f); 

        float last_bsdf_pdf = 0.0f; 
        bool last_bounce_specular = true;

        for (int bounce = 0; bounce < max_depth; ++bounce) {
            HitRecord rec;
            
            // 1. Intersection
            if (!scene.intersect(current_ray, SHADOW_EPSILON, Infinity, rec)) {
                // Background usually treated as pure BSDF sample (or implement MIS for EnvMap later)
                L += throughput * scene.sample_background(current_ray);
                break;
            }

            // 2. Emission (Hit Light via BSDF sampling)
            if (rec.mat_ptr->is_emissive()) {
                glm::vec3 emitted = rec.mat_ptr->emitted(rec.u, rec.v, rec.p);
                
                // If it's the first bounce or the previous bounce was specular, 
                // we cannot use NEE, so we take full emission.
                // Otherwise, use MIS weight.
                if (bounce == 0) {
                    L += throughput * emitted;
                } else {
                    // We need to know: If we had used NEE, what was the probability of picking this light?
                    // NOTE: This assumes we pick 1 random light uniformly from N lights.
                    // light_pdf_nee = (1/N) * pdf_on_light
                    
                    // We only apply MIS if we have lights to sample.
                    if (!scene.lights.empty()) {
                        // Retrieve the PDF of sampling this specific point on the object via NEE
                        float pdf_light_area = rec.object->pdf_value(current_ray.origin(), current_ray.direction());
                        float pdf_light_selection = 1.0f / scene.lights.size();
                        float pdf_light = pdf_light_area * pdf_light_selection;
                        
                        // Retrieve the PDF of the BSDF that generated this ray (stored in previous loop or implied)
                        // Actually, we need to pass the BSDF pdf from the previous bounce to here.
                        // However, simpler standard implementation calculates weight at the moment of scattering.
                        // Here we use a slightly different flow: we apply weight to the emission *now*.
                        // But wait, we don't have the `bsdf_pdf` of the *previous* ray here easily unless passed.
                        
                        // Standard Approach: Accumulate L inside the loop where scatter happens? 
                        // No, let's just use a cached variable `last_bsdf_pdf`.
                        
                        float weight = power_heuristic(last_bsdf_pdf, pdf_light);
                        if (last_bounce_specular) weight = 1.0f; // Specular path has no NEE chance

                        L += throughput * emitted * weight;
                    } else {
                         L += throughput * emitted;
                    }
                }
                // Emission stops the path for opaque lights usually, but let's break.
                break; 
            }

            ScatterRecord srec(rec.normal);
            if (!rec.mat_ptr->scatter(current_ray, rec, srec)) {
                break;
            }
            
            // Store for next iteration's MIS weight calculation
            last_bsdf_pdf = srec.pdf; 
            last_bounce_specular = srec.is_specular;

            // 3. Direct Light Sampling (Next Event Estimation)
            if (!srec.is_specular && !scene.lights.empty()) {
                int light_idx = random_int(0, static_cast<int>(scene.lights.size()) - 1);
                auto light = scene.lights[light_idx];
                float light_weight = static_cast<float>(scene.lights.size()); // 1 / (1/N)

                glm::vec3 to_light;
                float light_pdf; // PDF of sampling this point on light
                float dist;
                
                glm::vec3 L_emitted = light->sample_li(rec.p, to_light, light_pdf, dist);

                if (light_pdf > EPSILON && !near_zero(L_emitted)) {
                    Ray shadow_ray(rec.p, to_light, current_ray.time());
                    HitRecord shadow_rec;
                    
                    if (!scene.intersect(shadow_ray, SHADOW_EPSILON, dist - SHADOW_EPSILON, shadow_rec)) {
                        glm::vec3 f_r = rec.mat_ptr->eval(current_ray, rec, shadow_ray);
                        
                        if (!near_zero(f_r)) {
                            // Calculate BSDF PDF for this NEE direction
                            float bsdf_pdf = rec.mat_ptr->scattering_pdf(current_ray, rec, shadow_ray);
                            
                            // MIS Weight: balance Light sampling vs BSDF sampling
                            // We selected a specific light (prob 1/N), then a point on it (light_pdf).
                            // Total light technique PDF = (1/N) * light_pdf
                            float total_light_pdf = light_pdf / light_weight; 

                            float weight = power_heuristic(total_light_pdf, bsdf_pdf);

                            float cos_theta = glm::dot(srec.shading_normal, glm::normalize(to_light));
                            if (cos_theta > 0.0f) {
                                if (glm::dot(rec.normal, glm::normalize(to_light)) > 0.0f) 
                                    L += throughput * L_emitted * f_r * cos_theta * weight / total_light_pdf;
                            }
                        }
                    }
                }
            }

            // 4. Update Throughput for Indirect Bounce
            if (srec.is_specular) {
                throughput *= srec.attenuation;
            } else {
                if (srec.pdf > EPSILON) {
                    glm::vec3 f_r = rec.mat_ptr->eval(current_ray, rec, srec.specular_ray);
                    float cos_theta = std::abs(glm::dot(rec.normal, srec.specular_ray.direction()));
                    throughput *= f_r * cos_theta / srec.pdf;
                } else {
                    break;
                }
            }
            
            current_ray = srec.specular_ray;

            // Russian Roulette
            if (bounce > 3) {
                float p = std::max(throughput.x, std::max(throughput.y, throughput.z));
                if (random_float() > p) break;
                throughput /= p;
            }
        }
        return L;
    }

private:
    int max_depth;
};