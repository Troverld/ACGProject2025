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

    virtual glm::vec3 estimate_radiance(const Ray& r, const Scene& scene, int max_depth) const override {
        Ray current_ray = r;
        glm::vec3 throughput(1.0f);
        glm::vec3 radiance(0.0f);

        for (int depth = 0; depth < max_depth; ++depth) {
            HitRecord rec;
            // 1. Intersect
            if (!scene.intersect(current_ray, SHADOW_EPSILON, Infinity, rec)) {
                // Miss: Get environment light
                radiance += throughput * scene.sample_background(current_ray);
                break; // Exit loop
            }

            // 2. Emission (if logic exists)
            glm::vec3 emitted = rec.mat_ptr->emitted(rec.u, rec.v, rec.p);
            radiance += throughput * emitted;

            // 3. Scatter
            Ray scattered;
            glm::vec3 attenuation;
            if (rec.mat_ptr->scatter(current_ray, rec, attenuation, scattered)) {
                throughput *= attenuation; // Apply color attenuation
                current_ray = scattered;   // Next bounce
                
                // Optimization: Russian Roulette can be added here
            } else {
                break; // Absorbed
            }
        }
        return radiance;
    }

private:
    int max_depth;
};