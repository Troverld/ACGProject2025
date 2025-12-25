#pragma once
#include "integrator_utils.hpp"

/**
 * @brief Path Tracer with Multiple Importance Sampling (MIS).
 */
class PathIntegrator : public Integrator {
public:
    PathIntegrator(int max_d, const Scene& scene) : max_depth(max_d) {preprocess(scene);}

    glm::vec3 estimate_radiance(const Ray& start_ray, const Scene& scene) const {
        Ray current_ray = start_ray;
        glm::vec3 L(0.0f);           
        glm::vec3 throughput(1.0f); 

        float last_bsdf_pdf = 0.0f; 
        bool last_bounce_specular = true;

        for (int bounce = 0; bounce < max_depth; ++bounce) {
            HitRecord rec;
            
            // 1. Intersection
            if (!scene.intersect(current_ray, SHADOW_EPSILON, Infinity, rec)) {
                L += throughput * eval_environment(scene, current_ray, last_bsdf_pdf, last_bounce_specular);
                break;
            }

            // 2. Emission (Hit Light via BSDF sampling)
            if (rec.mat_ptr->is_emissive()) {
                glm::vec3 e = throughput * eval_emission(scene, rec, current_ray, last_bsdf_pdf, last_bounce_specular);
                if (bounce > 0) clamp_radiance(e);
                L += e;
                break; // Stop at light source
            }

            // 3. Material Sampling
            ScatterRecord srec(rec.normal);
            if (!rec.mat_ptr->scatter(current_ray, rec, srec)) break;

            // 4. Direct Lighting via NEE (if not specular)
            if (!srec.is_specular) {
                glm::vec3 e = throughput * sample_one_light(scene, rec, srec, current_ray, false); // need all lights
                clamp_radiance(e);
                L += e;
            }
            
            // 5. Update Throughput for Indirect Bounce
            if (srec.is_specular) {
                throughput *= srec.attenuation;
                last_bsdf_pdf = 1.0f; // Dirac distribution, arbitrary placeholder
            } else {
                float cos_theta = std::abs(glm::dot(srec.shading_normal, srec.specular_ray.direction()));
                throughput *= rec.mat_ptr->eval(current_ray, rec, srec.specular_ray, srec.shading_normal) * cos_theta / srec.pdf;
                last_bsdf_pdf = srec.pdf; 
            }
            
            current_ray = srec.specular_ray;
            last_bounce_specular = srec.is_specular;

            // 6. Russian Roulette
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