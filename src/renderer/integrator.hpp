#pragma once

#include "../scene/scene.hpp"
#include "../scene/camera.hpp"
#include "../material/material.hpp"
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
    virtual glm::vec3 estimate_radiance(const Ray& r, const Scene& scene, int depth) const = 0;
};

/**
 * @brief Standard Unidirectional Path Tracer.
 */
class PathIntegrator : public Integrator {
public:
    PathIntegrator(int max_d) : max_depth(max_d) {}

    /**
     * @brief Estimate radiance using Next Event Estimation (NEE).
     * 1. Direct Lighting (explicitly sample light sources).
     * 2. Indirect Lighting (BSDF sampling).
     */
    glm::vec3 estimate_radiance(const Ray& start_ray, const Scene& scene, int depth) const {
        Ray current_ray = start_ray;
        glm::vec3 L(0.0f);           
        glm::vec3 throughput(1.0f); 
        bool last_bounce_was_specular = true; // the first frame is considered specular.

        for (int bounce = 0; bounce < max_depth; ++bounce) {
            HitRecord rec;
            
            // 1. Intersection
            if (!scene.intersect(current_ray, SHADOW_EPSILON, Infinity, rec)) {
                L += throughput * scene.sample_background(current_ray);
                break;
            }

            // 2. Emission
            if (rec.mat_ptr->is_emissive()) {
                // If we hit a light source:
                // Case First bounce (direct view) & Specular bounce (e.g. mirror) -> Add it (because Metal::eval is 0, NEE didn't account for it).
                // Diffuse bounce -> Ignore it (Assumed covered by NEE).
                if (last_bounce_was_specular) {
                    L += throughput * rec.mat_ptr->emitted(rec.u, rec.v, rec.p);
                }
                break; 
            }

            ScatterRecord srec;
            if (!rec.mat_ptr->scatter(current_ray, rec, srec)) {
                break; // no scatter, just absorb
            }
            // 3. Direct Light Sampling (Next Event Estimation)
            if (!srec.is_specular && !scene.lights.empty()) {
                // Randomly pick a light
                int light_idx = random_int(0, static_cast<int>(scene.lights.size()) - 1);
                auto light = scene.lights[light_idx];
                
                // Weight for uniform light selection (1 / (1/N)) = N
                float light_weight = static_cast<float>(scene.lights.size());

                glm::vec3 to_light;
                float light_pdf;
                float dist;
                
                // A. Sample point on light
                glm::vec3 L_emitted = light->sample_li(rec.p, to_light, light_pdf, dist);

                if (light_pdf > EPSILON && !near_zero(L_emitted)) {
                    Ray shadow_ray(rec.p, to_light, current_ray.time());
                    HitRecord shadow_rec;
                    
                    // B. Visibility Test
                    if (!scene.intersect(shadow_ray, SHADOW_EPSILON, dist - SHADOW_EPSILON, shadow_rec)) {
                        
                        // C. Evaluate BSDF (Material Response)
                        // Ask material: "How much light reflects from 'to_light' towards 'current_ray'?"
                        glm::vec3 f_r = rec.mat_ptr->eval(current_ray, rec, shadow_ray);

                        if (!near_zero(f_r)) {
                            // D. Geometric Term (Cosine Law)
                            float cos_theta = glm::dot(rec.normal, glm::normalize(to_light));
                            if (cos_theta > 0.0f) {
                                // Rendering Equation (Monte Carlo Estimate for Direct Light):
                                // L_dir = (Emitted * BRDF * CosTheta) / PDF
                                L += throughput * L_emitted * f_r * cos_theta * light_weight / light_pdf;
                            }
                        }
                    }
                }
            }

            // 4. Indirect Bounce
            if (srec.is_specular) {
                throughput *= srec.attenuation;
            } else {
                if (srec.pdf > EPSILON) {
                    glm::vec3 f_r = rec.mat_ptr->eval(current_ray, rec, srec.specular_ray);
                    float cos_theta = std::abs(glm::dot(rec.normal, srec.specular_ray.direction()));
                    throughput *= f_r * cos_theta / srec.pdf;
                } else {
                    // Fallback: if PDF = 0 but the material is not specular, usually an error, just use attenuation.
                    throughput *= srec.attenuation; 
                }
            }
            
            current_ray = srec.specular_ray;
            last_bounce_was_specular = srec.is_specular;
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