#pragma once

#include "integrator_utils.hpp"
#include "../accel/kdtree.hpp"
#include "../core/photon.hpp"
#include "../core/onb.hpp"
#include <omp.h>
#include <iostream>
#include <vector>
#include <mutex>
#include <algorithm>

/**
 * @brief Integrator implementing Photon Mapping with MIS-based Direct Lighting.
 * 
 * Logic Overview for "Double Counting" Prevention:
 * 1. Direct Lighting (L -> D): Handled by NEE + BSDF Sampling (MIS). Not stored in Photon Maps.
 * 2. Caustics (L -> ... -> S -> D): Handled by Caustic Map lookup on Diffuse surfaces.
 * 3. Indirect Diffuse (L -> ... -> D -> D): 
 *    - Early bounces: Path Tracing recursion.
 *    - Late bounces: Global Map lookup (Final Gather).
 * 
 * "Sticky Flag" Strategy:
 * To strictly prevent double counting between the Caustic Map and Path Tracing:
 * - When a ray leaves a Diffuse surface and hits a Specular surface, it enters a "Caustic Path".
 * - This "in_caustic_path" flag is sticky (permanent for that ray branch).
 * - If a ray with this flag hits a Local Light, the contribution is DISCARDED (because the 
 *   energy was already retrieved from the Caustic Map at the Diffuse origin).
 * - Environment lights remain valid as they are not in the photon map.
 */
class PhotonIntegrator : public Integrator {
public:
    /**
     * @param max_d Maximum bounce depth for both photons and camera rays.
     * @param n_photons Number of photons to emit globally.
     * @param caustic_r Radius to search for caustic photons.
     * @param global_r Radius to search for global indirect photons.
     * @param f_gather_bound Depth at which to switch from PT recursion to Global Map lookup.
     */
    PhotonIntegrator(int max_d, int n_photons,
                     float caustic_r, float global_r, int k,
                     int f_gather_bound, const Scene& scene) 
        : max_depth(max_d), num_photons_global(n_photons), 
          gather_radius_caustic(caustic_r), gather_radius_global(global_r), 
          K(k),
          final_gather_bound(f_gather_bound) {
            build_photon_map(scene);
        }

    /**
     * @brief Phase 1: Emit photons and build the KD-Trees.
     */
    void build_photon_map(const Scene& scene) {
        if (scene.lights.empty()) {
            std::cout << "[PhotonIntegrator] Warning: No lights in scene. Photon map will be empty.\n";
            return;
        }

        std::cout << "[PhotonIntegrator] Emitting " << num_photons_global << " photons..." << std::endl;

        int num_lights = static_cast<int>(scene.lights.size());
        int photons_per_light = num_photons_global / num_lights;
        int caustic_photons_per_target = 0;

        std::vector<const Object*> targets = find_specular_targets(scene);
        if (targets.empty()) {
            std::cout << "No specular targets found for guided emission." << std::endl;
        } else {
            photons_per_light = (num_photons_global / 2) / num_lights;
            // photons_per_light = 0;
            caustic_photons_per_target = (num_photons_global / 2) / (int)(targets.size() * num_lights);
        }

        // Thread-safe temporary storage
        std::vector<Photon> master_caustic_list;
        std::vector<Photon> master_global_list;
        std::mutex list_mutex;

        #pragma omp parallel
        {
            std::vector<Photon> local_caustic;
            std::vector<Photon> local_global;

            #pragma omp for schedule(dynamic)
            for (int i = 0; i < num_lights; ++i) {
                const auto& light = scene.lights[i];

                for (int k = 0; k < photons_per_light; ++k) {
                    glm::vec3 pos, dir, power;
                    light->emit(pos, dir, power, static_cast<float>(photons_per_light)); 

                    if (glm::length(power) > 0.0f) {
                        Ray photon_ray(pos + dir * SHADOW_EPSILON, dir); 
                        trace_photon(scene, photon_ray, power, local_caustic, local_global);
                    }
                }

                for (const auto& target : targets) {
                    for (int k = 0; k < caustic_photons_per_target; ++k) {
                        glm::vec3 pos, dir, power;
                        if (light->emit_targeted(pos, dir, power, (float)caustic_photons_per_target, *target)
                            && glm::length(power) > 0.0f) {
                                Ray photon_ray(pos + dir * SHADOW_EPSILON, dir); 
                                trace_photon(scene, photon_ray, power, local_caustic, local_global);
                            }
                    }
                }
            }

            if (!local_caustic.empty() || !local_global.empty()) {
                std::lock_guard<std::mutex> lock(list_mutex);
                master_caustic_list.insert(master_caustic_list.end(), local_caustic.begin(), local_caustic.end());
                master_global_list.insert(master_global_list.end(), local_global.begin(), local_global.end());
            }
        }

        std::cout << "[PhotonIntegrator] Building KD-Trees... (Caustic: " 
                  << master_caustic_list.size() << ", Global: " << master_global_list.size() << ")" << std::endl;
        
        for(const auto& p : master_caustic_list) caustic_map.add_photon(p);
        for(const auto& p : master_global_list) global_map.add_photon(p);

        global_map.build();
        caustic_map.build();
    }

    /**
     * @brief Phase 2: Render Pixel (Hybrid Path Tracing + Photon Mapping).
     * Implements "Sticky Flag" logic to handle L-S-D paths correctly.
     */
    virtual glm::vec3 estimate_radiance(const Ray& start_ray, const Scene& scene) const override {
        glm::vec3 L(0.0f);
        glm::vec3 throughput(1.0f);
        Ray current_ray = start_ray;
        
        // Initial State
        bool last_bounce_specular = true; // Treats primary ray as specular for MIS
        bool in_caustic_path = false;     // "Sticky Flag": Once true, stays true.
        float last_bsdf_pdf = 0.0f;

        for (int bounce = 0; bounce < max_depth; ++bounce) {
            HitRecord rec;
            
            // -----------------------------------------------------------------
            // 1. Intersection & Environment
            // -----------------------------------------------------------------
            if (!scene.intersect(current_ray, SHADOW_EPSILON, Infinity, rec)) {
                // Environment light is NOT in the photon map.
                // Always evaluate it, regardless of in_caustic_path state.
                L += throughput * eval_environment(scene, current_ray, last_bsdf_pdf, last_bounce_specular);
                break;
            }

            // -----------------------------------------------------------------
            // 2. Emission (Hit Local Light)
            // -----------------------------------------------------------------
            if (rec.mat_ptr->is_emissive()) {
                if (in_caustic_path) {
                    // [DISCARD] 
                    // We are in a path: Diffuse_Origin -> Specular -> ... -> Light.
                    // The energy of this path was already retrieved at Diffuse_Origin 
                    // via the Caustic Map. Adding it here would be double counting.
                } 
                else {
                    // [KEEP]
                    // Standard Path Tracing / MIS logic.
                    // Handles L -> Camera, L -> Specular -> Camera, or L -> Diffuse (via BSDF sampling).
                    glm::vec3 e = throughput * eval_emission(scene, rec, current_ray, last_bsdf_pdf, last_bounce_specular);
                    if (bounce > 0) clamp_radiance(e);
                    L += e;
                }
                break; // Stop at light source
            }

            // -----------------------------------------------------------------
            // 3. Material Scatter
            // -----------------------------------------------------------------
            ScatterRecord srec(rec.normal);
            if (!rec.mat_ptr->scatter(current_ray, rec, srec)) break;

            // -----------------------------------------------------------------
            // 4. Handle Logic based on Material Type
            // -----------------------------------------------------------------
            if (srec.is_specular) {
                // === SPECULAR BOUNCE (Mirror/Glass) ===
                
                // [Update Sticky Flag]
                // If we transition from Diffuse to Specular, we enter the "Caustic Zone".
                // If we were already in it (S -> S), we stay in it.
                if (!last_bounce_specular) {
                    in_caustic_path = true;
                }
                
                // Pure Path Tracing logic for specular
                throughput *= srec.attenuation;
                current_ray = srec.specular_ray;
                last_bounce_specular = true;
                last_bsdf_pdf = 1.0f; // Dirac delta PDF
            }
            else {
                // === DIFFUSE BOUNCE ===
                
                if (in_caustic_path) {
                    // [CASE: Diffuse inside a Caustic Path]
                    // Path: ... -> Diffuse_Origin -> Specular -> ... -> Diffuse_Current
                    // The energy reaching here from Local Lights is effectively 
                    // part of the caustic computed at Diffuse_Origin.
                    
                    // A. NO NEE (Direct light is part of the previous caustic chain)
                    // B. NO Map Lookup (Double counting prevention)
                    // C. Recursion Allowed (To find Environment Light only)

                    float cos_theta = std::abs(glm::dot(srec.shading_normal, srec.specular_ray.direction()));
                    if (srec.pdf <= EPSILON) break;

                    glm::vec3 f_r = rec.mat_ptr->eval(current_ray, rec, srec.specular_ray, srec.shading_normal);
                    throughput *= (f_r * cos_theta / srec.pdf);
                    
                    current_ray = srec.specular_ray;
                    last_bounce_specular = false;
                    last_bsdf_pdf = srec.pdf;
                    // in_caustic_path remains TRUE (Sticky)
                } 
                else {
                    // [CASE: Standard Diffuse Surface]
                    // This is a primary gathering point (Eye -> ... -> D).
                    // We collect all incoming energy here.

                    // 1. Direct Light (NEE) - Handles L -> D
                    glm::vec3 L_direct = sample_one_light(scene, rec, srec, current_ray);
                    clamp_radiance(L_direct);
                    L += throughput * L_direct;

                    // 2. Caustics (Map) - Handles L -> ... -> S -> D
                    glm::vec3 L_caustic = estimate_radiance_from_map(rec, srec.attenuation, caustic_map, gather_radius_caustic);
                    clamp_radiance(L_caustic);
                    L += throughput * L_caustic;

                    // 3. Indirect Diffuse
                    if (bounce >= final_gather_bound) {
                        // [Termination] Global Map - Handles L -> ... -> D -> D
                        glm::vec3 L_indirect = estimate_radiance_from_map(rec, srec.attenuation, global_map, gather_radius_global);
                        clamp_radiance(L_indirect);
                        L += throughput * L_indirect;
                        break; // Stop recursion
                    } 
                    else {
                        // [Recursion] Path Tracing
                        float cos_theta = std::abs(glm::dot(srec.shading_normal, srec.specular_ray.direction()));
                        if (srec.pdf <= EPSILON) break;

                        glm::vec3 f_r = rec.mat_ptr->eval(current_ray, rec, srec.specular_ray, srec.shading_normal);
                        throughput *= (f_r * cos_theta / srec.pdf);

                        current_ray = srec.specular_ray;
                        last_bounce_specular = false; // Next hit will see this as Diffuse
                        last_bsdf_pdf = srec.pdf;
                        // in_caustic_path remains FALSE
                    }
                }
            }

            // -----------------------------------------------------------------
            // 5. Russian Roulette
            // -----------------------------------------------------------------
            if (bounce > 3) {
                float p = std::max(throughput.x, std::max(throughput.y, throughput.z));
                p = std::clamp(p, 0.05f, 0.95f);
                if (random_float() > p) break;
                throughput /= p;
            }
        }
        return L;
    }

private:
    int max_depth;
    int final_gather_bound;
    int num_photons_global;
    int K;
    float time0;
    float gather_radius_global;
    float gather_radius_caustic;
    PhotonMap global_map;
    PhotonMap caustic_map;

    std::vector<const Object*> find_specular_targets(const Scene& scene) {
        std::vector<const Object*> targets;
        for (const auto& obj : scene.objects) {
            if (obj->get_material()->is_specular()) {
                targets.push_back(obj.get());
            }
        }
        return targets;
    }

    /**
     * @brief Traces a photon.
     * Logic:
     * - Specular: Reflect/Refract, don't store.
     * - Diffuse (via Specular): Store in Caustic Map.
     * - Diffuse (via Diffuse): Store in Global Map (if depth > 0 to exclude Direct Light).
     */
    void trace_photon(const Scene& scene, Ray r, glm::vec3 power, 
                      std::vector<Photon>& local_caustic, 
                      std::vector<Photon>& local_global) const {
        
        int depth = 0;
        bool prev_bounce_specular = false; // Emission is not specular

        while (depth < max_depth) {
            HitRecord rec;
            if (!scene.intersect(r, SHADOW_EPSILON, Infinity, rec)) break;

            ScatterRecord srec(rec.normal);
            if (!rec.mat_ptr->scatter(r, rec, srec)) break;

            if (srec.is_specular) {
                // Pass energy through specular
                power *= srec.attenuation;
                r = srec.specular_ray;
                depth++;
                prev_bounce_specular = true;
            } 
            else {
                // Hit Diffuse
                if (prev_bounce_specular) {
                    // Path: Light -> ... -> Specular -> Diffuse (Caustics)
                    local_caustic.push_back({rec.p, power, -glm::normalize(r.direction())});
                } 
                else if (depth > 0) {
                    // Path: Light -> Diffuse -> ... -> Diffuse (Indirect Global)
                    // Depth > 0 ensures we don't store Direct Lighting (L -> D)
                    local_global.push_back({rec.p, power, -glm::normalize(r.direction())});
                }

                // Russian Roulette
                float max_albedo = std::max({srec.attenuation.r, srec.attenuation.g, srec.attenuation.b});
                float q = std::clamp(max_albedo, 0.0f, 1.0f); 
                if (random_float() > q) break;

                // Scatter
                if (q > EPSILON) power *= (srec.attenuation / q);
                r = srec.specular_ray;
                depth++;
                prev_bounce_specular = false;
            }
        }
    }

    /**
     * @brief Estimates radiance from map using kNN and Cone Filter.
     */
    glm::vec3 estimate_radiance_from_map(const HitRecord& rec, const glm::vec3& albedo, const PhotonMap& map, float radius) const {
        static thread_local std::vector<NearPhoton> neighbors;
        float max_dist_sq = radius * radius;

        // 1. Perform K-Nearest Neighbor Search
        map.find_knn(rec.p, K, neighbors, max_dist_sq);

        if (neighbors.empty()) return glm::vec3(0.0f);

        float max_dist = std::sqrt(max_dist_sq);
        glm::vec3 flux_sum(0.0f);

        // 2. Accumulate weighted flux using Cone Filter
        // Formula: Weight = 1 - (dist / max_dist)
        for (const auto& np : neighbors) {
            const Photon* p = np.photon;            
            // Leak prevention
            if (glm::dot(rec.normal, p->incoming) < 0.0f) continue;

            float dist = std::sqrt(np.dist_sq);
            float weight = 1.0f - (dist / max_dist); 
            flux_sum += p->power * weight;
        }

        // 3. Normalization
        // For cone filter, the normalization area is modified.
        // Factor derived from Jensen's "Global Illumination using Photon Maps"
        // Area = (1 - 2/3k) * PI * r^2
        float area = PI * max_dist_sq;
        float normalization_factor = (1.0f - 2.0f / (3.0f * (float)neighbors.size())) * area;
        return (flux_sum * albedo) / (normalization_factor * PI);
    }
};