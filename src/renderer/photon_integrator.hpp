#pragma once

#include "integrator.hpp"
#include "../accel/kdtree.hpp"
#include "../core/photon.hpp"
#include "../core/onb.hpp"
#include <omp.h>
#include <iostream>
#include <vector>

/**
 * @brief Integrator implementing Photon Mapping with correct caustics support.
 * 
 * Logic Flow:
 * 1. Preprocess: Emit photons from Area Lights using surface sampling.
 * 2. Trace: Photons bounce iteratively. 
 *    - Specular surfaces: Pass through (do not store).
 *    - Diffuse surfaces: Store in Photon Map, then chance to scatter (Russian Roulette).
 * 3. Render: 
 *    - Direct Light: via NEE (PathIntegrator logic).
 *    - Caustics/Indirect Diffuse: via Photon Density Estimation.
 */
class PhotonIntegrator : public PathIntegrator {
public:
    /**
     * @param max_d Maximum bounce depth for both photons and camera rays.
     * @param n_photons Number of photons to emit globally.
     * @param gather_r Radius to search for photons during rendering (Density Estimation).
     */
    PhotonIntegrator(int max_d, int n_photons, float caustic_r, float global_r, int f_gather_bound, const Scene& scene) 
        : PathIntegrator(max_d), num_photons_global(n_photons), gather_radius_caustic(caustic_r), gather_radius_global(global_r), final_gather_bound(f_gather_bound) {
            build_photon_map(scene);
        }

    /**
     * @brief Phase 1: Emit photons and build the KD-Tree.
     * Call this before rendering the image.
     */
    void build_photon_map(const Scene& scene) {
        if (scene.lights.empty()) {
            std::cout << "[PhotonIntegrator] Warning: No lights in scene. Photon map will be empty.\n";
            return;
        }

        std::cout << "[PhotonIntegrator] Emitting " << num_photons_global << " photons..." << std::endl;

        // Distribute photons among lights.
        // Simplification: Uniform distribution.
        // Better approach: Distribute based on light power (Flux).
        int num_lights = static_cast<int>(scene.lights.size());
        int photons_per_light = num_photons_global / num_lights;

        // We use a local buffer for each thread to avoid high contention on the global photon list
        // However, PhotonMap::add_photon is not thread safe, so we will use a vector of vectors or a critical section.
        // Here we just use a critical section for simplicity as emission calculation is the bottleneck, not insertion.
        
        #pragma omp parallel for schedule(dynamic)
        for (int i = 0; i < num_lights; ++i) {
            const auto& light = scene.lights[i];

            for (int k = 0; k < photons_per_light; ++k) {
                glm::vec3 pos, dir, power;
                
                light->emit(pos, dir, power, static_cast<float>(photons_per_light)); 

                if (glm::length(power) > 0.0f) {
                    Ray photon_ray(pos + dir * SHADOW_EPSILON, dir); 
                    trace_photon(scene, photon_ray, power);
                }
            }
        }

        std::cout << "[PhotonIntegrator] Building KD-Tree..." << std::endl;
        global_map.build();
        caustic_map.build();
    }

    /**
     * @brief Phase 2: Render Pixel.
     * Overrides PathIntegrator::estimate_radiance.
     */
    virtual glm::vec3 estimate_radiance(const Ray& start_ray, const Scene& scene) const override {
        glm::vec3 L(0.0f);
        glm::vec3 throughput(1.0f);
        Ray current_ray = start_ray;
        bool last_bounce_specular = true;

        for (int bounce = 0; bounce < max_depth; ++bounce) {
            HitRecord rec;
            if (!scene.intersect(current_ray, SHADOW_EPSILON, Infinity, rec)) {
                glm::vec3 nee_contribution = throughput * scene.sample_background(current_ray);
                clamp_radiance(nee_contribution);
                L += nee_contribution;
                break;
            }

            // 1. Direct Emission
            if (rec.mat_ptr->is_emissive()) {
                if (last_bounce_specular) {
                    glm::vec3 nee_contribution = throughput * rec.mat_ptr->emitted(rec.u, rec.v, rec.p);
                    if (bounce > 0)
                        clamp_radiance(nee_contribution);
                    L += nee_contribution;
                }
                break;
            }

            ScatterRecord srec(rec.normal);
            if (!rec.mat_ptr->scatter(current_ray, rec, srec))
                break;

            // 2. Specular Surface (Mirror / Glass)
            // Photons are not stored on specular surfaces, so we must trace rays recursively
            // to see what's reflected/refracted in the mirror.
            if (srec.is_specular) {
                throughput *= srec.attenuation;
                current_ray = srec.specular_ray;
                last_bounce_specular = true;
            } else {
                // 3. Diffuse Surface
                // Composition: Direct Light (NEE) + Indirect Light (Photon Map)

                // A. Direct Light (Next Event Estimation)
                // We reuse the logic from PathIntegrator parent class
                glm::vec3 nee_contribution = throughput * sample_one_light(scene, rec, srec, current_ray);
                if (bounce > 0)
                    clamp_radiance(nee_contribution);
                L += nee_contribution;
                // B: Caustic Light 
                nee_contribution = throughput * estimate_radiance_from_map(rec, srec.attenuation, caustic_map, gather_radius_caustic);
                if (bounce > 0)
                    clamp_radiance(nee_contribution);
                L += nee_contribution;
                // C. Indirect Light (Photon Density Estimation)
                if (bounce < final_gather_bound) {
                    float cos_theta = std::abs(glm::dot(srec.shading_normal, srec.specular_ray.direction()));
                    glm::vec3 f_r = rec.mat_ptr->eval(current_ray, rec, srec.specular_ray, srec.shading_normal);
                    if (srec.pdf > EPSILON) {
                        throughput *= (f_r * cos_theta / srec.pdf);
                        current_ray = srec.specular_ray;
                        last_bounce_specular = false;
                    } else break;
                } else {
                    nee_contribution = throughput * estimate_radiance_from_map(rec, srec.attenuation, global_map, gather_radius_global);
                    clamp_radiance(nee_contribution);
                    L += nee_contribution;
                    break;
                }
            }
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
    glm::vec3 estimate_radiance_from_map(const HitRecord& rec, const glm::vec3& annuation, const PhotonMap& map, float radius) const {
        std::vector<const Photon*> photons;
        map.find_in_radius(rec.p, radius, photons);

        if (photons.empty())
            return glm::vec3(0.0f);
        glm::vec3 flux_sum(0.0f);
        
        // Kernel estimation (Simple constant kernel)
        for (const auto* p : photons) {
            // Optional: Filter photons that hit from the "wrong" side (backface)
            // if (glm::dot(rec.normal, p->incoming) > 0.0f) 
            float dist = glm::distance(rec.p, p->p);
            if (glm::dot(rec.normal, p->incoming) > 0.0f && dist < radius) {
                flux_sum += p->power * (1.0f - dist / radius);
            }
        }

        // Radiance Estimate formula for Diffuse surface:
        // L = (Sum(Flux) / (PI * r^2)) * (Albedo / PI)
        // Note: The BRDF for Lambertian is Albedo/PI.
        // The Area is PI*r^2.
        // Factor PI^2 in denominator? 
        // Standard derivation: Reflected_Flux = Sum(Incoming_Flux) * Albedo.
        // L_out = Reflected_Flux / (Area * PI) [Assuming Lambertian distribution of outgoing light]
        // So: L = (Sum(Flux) * Albedo) / (PI * r^2 * PI)
        // Use Cone Filter, then it's (Sum(Flux) * 3 * Albedo) / (PI * r^2 * PI)
        
        float area = PI * radius * radius;
        return (flux_sum * 3.0f * annuation) / (area * PI);
    }
    /**
     * @brief Iteratively traces a single photon through the scene.
     * Handles Caustics by passing through specular surfaces and storing on diffuse ones.
     */
    void trace_photon(const Scene& scene, Ray r, glm::vec3 power) {
        int depth = 0;
        bool has_hit_specular = false;

        while (depth < max_depth) {
            HitRecord rec;
            if (!scene.intersect(r, SHADOW_EPSILON, Infinity, rec)) {
                break; // Missed scene
            }

            ScatterRecord srec(rec.normal);
            if (!rec.mat_ptr->scatter(r, rec, srec)) {
                break; // Absorbed
            }

            if (srec.is_specular) {
                // --- Case: Specular (Glass/Mirror) ---
                // Do NOT store photon here.
                // Just bounce it. The power is attenuated by the surface color.
                // This allows the photon to travel through glass and eventually hit a diffuse floor (Caustics).
                
                power *= srec.attenuation;
                r = srec.specular_ray;
                depth++;
                has_hit_specular = true;
                
                // Specular surfaces usually don't absorb much in RR, or we skip RR to ensure caustics form.
                // We just continue loop.
            } else {
                // --- Case: Diffuse ---
                // 1. Store the photon
                // Critical section needed because pmap is shared
                if (has_hit_specular) {
                    #pragma omp critical
                    {
                        Photon p;
                        p.p = rec.p;
                        p.power = power;
                        p.incoming = -glm::normalize(r.direction());
                        caustic_map.add_photon(p);
                    }
                    break;
                } else if (depth > 0) {
                    #pragma omp critical
                    {
                        Photon p;
                        p.p = rec.p;
                        p.power = power;
                        p.incoming = -glm::normalize(r.direction());
                        global_map.add_photon(p);
                    }
                }

                // 2. Russian Roulette (RR)
                // Decide whether to absorb or reflect diffusely.
                // Probability of survival based on reflectance (Albedo brightness)
                float max_albedo = std::max({srec.attenuation.r, srec.attenuation.g, srec.attenuation.b});
                float q = std::clamp(max_albedo, 0.05f, 0.95f); // Probability to survive

                if (random_float() > q) {
                    break; // Absorbed
                }

                // 3. Scatter Diffusely
                // If it survives, we boost the power to compensate for the ones that died (Energy Conservation).
                // New Power = Old Power * Albedo / Probability
                power *= srec.attenuation / q;
                r = srec.specular_ray;
                depth++;
            }
        }
    }

private:
    int final_gather_bound;
    int num_photons_global;
    float gather_radius_global;
    float gather_radius_caustic;
    PhotonMap global_map;
    PhotonMap caustic_map;
};