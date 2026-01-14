#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <omp.h>
#include <iomanip>
#include <sstream>
#include <atomic>
#include <mutex>
#include <cmath>

// External
#include <glm/glm.hpp>
#include "stb_image_write.h"
#include "stb_image.h"

// Project Headers
#include "core/utils.hpp"
#include "core/ray.hpp"
#include "scene/scene.hpp"
#include "scene/camera.hpp"
#include "renderer/path_integrator.hpp"
#include "renderer/photon_integrator.hpp"

// Scene List
#include "scene_list.cpp"

struct RenderConfig {
    // --- Basic Image Settings ---
    int width;
    float aspect_ratio;
    
    // --- Sampling Settings ---
    int samples_per_pixel;
    int samples_per_batch;
    int max_depth;

    // --- Adaptive Sampling (Dynamic SPP) ---
    bool use_adaptive_sampling;
    float adaptive_threshold;
    int min_samples;

    // --- Integrator Type ---
    bool use_photon_mapping; 

    // --- Photon Mapping Specifics ---
    int num_photons;
    float caustic_radius;
    float global_radius;
    int k_nearest;
    int final_gather_bound;
};

// 默认配置生成器
RenderConfig get_default_config() {
    return {
        1188, 297.0f/210.0f,    // width, aspect
        5000, 50, 10,           // samples (max), batch, depth
        true, 0.01f, 64,        // [Dynamic] adaptive=true, threshold=0.01, min=64
        false,                  // use_photon_mapping
        5000000, 0.1f, 0.4f, 200, 4 // default photon settings
    };
}

// ==========================
// SCENE SELECTION
// ==========================
const int SCENE_ID = 7;

float get_luminance(const glm::vec3& color) {
    return glm::dot(color, glm::vec3(0.2126f, 0.7152f, 0.0722f));
}

void save_snapshot(int max_samples_reached, int width, int height, 
                   const std::vector<glm::vec3>& accum_buffer,
                   const std::vector<int>& pixel_counts,
                   bool save_heatmap = false) {
    
    std::vector<unsigned char> image_output(width * height * 3);
    std::vector<unsigned char> heatmap_output(width * height * 3);
    
    #pragma omp parallel for schedule(dynamic)
    for (int j = 0; j < height; ++j) {
        for (int i = 0; i < width; ++i) {
            int index = j * width + i;
            
            int N = pixel_counts[index];
            if (N == 0) N = 1;

            // Average the accumulated color
            glm::vec3 color = accum_buffer[index] / float(N);

            // Gamma Correction (Gamma 2.0 approximation)
            color = glm::sqrt(color);

            // Clamp and convert
            int out_idx = index * 3;
            image_output[out_idx + 0] = static_cast<unsigned char>(255.999f * std::clamp(color.r, 0.0f, 1.0f));
            image_output[out_idx + 1] = static_cast<unsigned char>(255.999f * std::clamp(color.g, 0.0f, 1.0f));
            image_output[out_idx + 2] = static_cast<unsigned char>(255.999f * std::clamp(color.b, 0.0f, 1.0f));

            // --- Generate Heatmap ---
            if (save_heatmap) {
                float ratio = (float)N / max_samples_reached;
                ratio = std::clamp(ratio, 0.0f, 1.0f);
                
                heatmap_output[out_idx + 0] = static_cast<unsigned char>(255.99f * ratio);          // R
                heatmap_output[out_idx + 1] = static_cast<unsigned char>(255.99f * (1.0f - ratio)); // G
                heatmap_output[out_idx + 2] = 0;                                                    // B
            }
        }
    }

    std::stringstream ss;
    ss << "output_scene_" << SCENE_ID << "_samples_" << std::setw(4) << std::setfill('0') << max_samples_reached << ".png";
    std::string filename = ss.str();

    stbi_write_png(filename.c_str(), width, height, 3, image_output.data(), width * 3);

    if (save_heatmap) {
        std::stringstream ss_heat;
        ss_heat << "heatmap_scene_" << SCENE_ID << "_samples_" << std::setw(4) << std::setfill('0') << max_samples_reached << ".png";
        stbi_write_png(ss_heat.str().c_str(), width, height, 3, heatmap_output.data(), width * 3);
    }

    std::cout << "\r[Snapshot] Saved: " << filename << std::string(20, ' ') << std::endl;
}

int main() {

    Scene world;
    Camera cam(glm::vec3(0), glm::vec3(0,0,-1), glm::vec3(0,1,0), 90, 16.0f/9.0f); 
    RenderConfig config = get_default_config();

    switch (SCENE_ID) {
        case 1: scene_materials_textures(world, cam, config.aspect_ratio); break;
        case 2:
            config.width = 600;
            config.aspect_ratio = 1.0f;
            config.use_photon_mapping = true;
            config.num_photons = 50000000;
            config.caustic_radius = 1.0f;
            config.global_radius = 4.0f;
            config.k_nearest = 200;
            config.final_gather_bound = 5;
            scene_cornell_smoke_caustics(world, cam, config.aspect_ratio);
            break;
        case 3: scene_motion_blur(world, cam, config.aspect_ratio); break;
        case 4: scene_mesh_env(world, cam, config.aspect_ratio); break;
        case 5: scene_5(world, cam, config.aspect_ratio); break;
        case 6: scene_dispersion(world, cam, config.aspect_ratio); break;
        case 7:
            config.use_photon_mapping = true;
            config.width = 1782;
            
            // --- Adaptive Settings ---
            config.use_adaptive_sampling = true;
            config.samples_per_batch = 50;
            config.samples_per_pixel = 5000;
            config.adaptive_threshold = 0.008f;
            config.min_samples = 50;

            config.num_photons = 300000000;
            config.caustic_radius = 0.3f;
            config.global_radius = 0.4f;
            config.k_nearest = 100;
            config.final_gather_bound = 5;
            scene_prism_spectrum(world, cam, config.aspect_ratio);
            break;
        case 8: scene_newton_test(world, cam, config.aspect_ratio); break;
        default: scene_materials_textures(world, cam, config.aspect_ratio); break;
    }

    const int width = config.width; 
    const int height = static_cast<int>(width / config.aspect_ratio); 
    
    std::cout << "Rendering Scene ID: " << SCENE_ID << " [" << width << "x" << height << "]" << std::endl;
    std::cout << "Max Samples: " << config.samples_per_pixel << " (Batch: " << config.samples_per_batch << ")" << std::endl;
    std::cout << "Adaptive Sampling: " << (config.use_adaptive_sampling ? "ON" : "OFF") << std::endl;

    world.build_bvh(0.0f, 1.0f); 

    std::unique_ptr<Integrator> integrator;

    if (config.use_photon_mapping) {
        std::cout << "Using Photon Integrator..." << std::endl;
        integrator = std::make_unique<PhotonIntegrator>(
            config.max_depth,
            config.num_photons,
            config.caustic_radius,
            config.global_radius,
            config.k_nearest,
            config.final_gather_bound,
            0.0f, 1.0f, world
        );
    } else {
        std::cout << "Using Path Integrator (MIS + NEE)..." << std::endl;
        integrator = std::make_unique<PathIntegrator>(config.max_depth, world);
    }

    // --- BUFFERS ---
    std::vector<glm::vec3> accumulation_buffer(width * height, glm::vec3(0.0f));
    
    std::vector<glm::vec3> accumulation_buffer_sq(width * height, glm::vec3(0.0f));

    std::vector<int> pixel_samples(width * height, 0);

    std::vector<bool> pixel_converged(width * height, false);

    std::atomic<int> total_active_pixels(width * height);
    int samples_loop_count = 0;

    // --- RENDER LOOP (Batched) ---
    while (samples_loop_count < config.samples_per_pixel) {
        
        if (total_active_pixels == 0) {
            std::cout << "\nAll pixels converged! Stopping early." << std::endl;
            break;
        }

        int current_batch_size = std::min(config.samples_per_batch, config.samples_per_pixel - samples_loop_count);
        
        std::atomic<int> completed_rows{0};
        std::mutex print_mutex;

        // Parallelize pixel loops
        #pragma omp parallel for schedule(dynamic)
        for (int j = 0; j < height; ++j) {

            for (int i = 0; i < width; ++i) {
                int index = j * width + i;

                if (config.use_adaptive_sampling && pixel_converged[index]) {
                    continue;
                }

                glm::vec3 batch_color(0.0f);
                glm::vec3 batch_color_sq(0.0f);

                // Run the samples for this batch
                for (int s = 0; s < current_batch_size; ++s) {
                    float u = (float(i) + random_float()) / width;
                    float v = (float(height - 1 - j) + random_float()) / height;

                    Ray r = cam.get_ray(u, v);
                    glm::vec3 rad = integrator->estimate_radiance(r, world);
                    
                    if (glm::any(glm::isnan(rad)) || glm::any(glm::isinf(rad))) rad = glm::vec3(0.0f);

                    batch_color += rad;
                    batch_color_sq += rad * rad;
                }

                // Update Buffers (Thread-safe due to distinct i, j ownership)
                accumulation_buffer[index] += batch_color;
                accumulation_buffer_sq[index] += batch_color_sq;
                pixel_samples[index] += current_batch_size;

                // --- Adaptive Sampling Convergence Check ---
                if (config.use_adaptive_sampling && pixel_samples[index] >= config.min_samples) {
                    float N = float(pixel_samples[index]);
                    
                    glm::vec3 mean = accumulation_buffer[index] / N;
                    glm::vec3 mean_sq = accumulation_buffer_sq[index] / N;

                    // Var(X) = E[X^2] - (E[X])^2
                    float lum_mean = get_luminance(mean);
                    float lum_mean_sq = get_luminance(mean_sq);
                    float variance = std::abs(lum_mean_sq - lum_mean * lum_mean);

                    // Standard Error (Standard Deviation of the Mean)
                    // Error = sqrt(Variance / N)
                    float error = std::sqrt(variance / N);

                    if (error < config.adaptive_threshold) {
                        pixel_converged[index] = true;
                        total_active_pixels--;
                    }
                }
            }
            
            completed_rows++;
        }

        samples_loop_count += current_batch_size;

        float active_percent = (float)total_active_pixels / (width * height) * 100.0f;
        std::cout << "\rBatch Done: " << std::setw(4) << samples_loop_count 
                  << " | Active: " << std::fixed << std::setprecision(1) << active_percent << "% "
                  << " | Snapshot Saving...    " << std::flush;

        save_snapshot(samples_loop_count, width, height, accumulation_buffer, pixel_samples, true);
    }

    save_snapshot(samples_loop_count, width, height, accumulation_buffer, pixel_samples, true);

    std::cout << "\n\nRendering Complete!" << std::endl;
    return 0;
}