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

void draw_progress_bar(int current, int total, int batch_idx, int active_px) {
    const int bar_width = 40;
    float progress = (total > 0) ? (float)current / total : 1.0f;
    int pos = static_cast<int>(bar_width * progress);

    std::stringstream ss;
    ss << "\rBatch " << std::setw(4) << batch_idx << " [";
    for (int i = 0; i < bar_width; ++i) {
        if (i < pos) ss << "=";
        else if (i == pos) ss << ">";
        else ss << " ";
    }
    ss << "] " << std::fixed << std::setprecision(1) << (progress * 100.0) << "% "
       << "| Active: " << active_px << " px    ";
    
    std::cout << ss.str() << std::flush;
}

std::string generate_filename(int scene_id, bool is_heatmap, const std::string& method, int spp, bool is_latest) {
    std::stringstream ss;
    // scene_[num]_[heatmap/output]_[PT/PM]_samples_[SPP].png
    ss << "scene_" << scene_id << "_"
       << (is_heatmap ? "heatmap" : "output") << "_"
       << method << "_samples_";
    
    if (is_latest) {
        ss << "latest";
    } else {
        ss << std::setw(5) << std::setfill('0') << spp;
    }
    ss << ".png";
    return ss.str();
}

/**
 * snapshot function
 * @param current_spp 
 * @param method_tag "PT" or "PM"
 * @param is_milestone
 */
void save_snapshot(int current_spp, int width, int height, 
                   const std::vector<glm::vec3>& accum_buffer,
                   const std::vector<int>& pixel_counts,
                   const std::string& method_tag,
                   bool is_milestone) {
    
    std::vector<unsigned char> image_output(width * height * 3);
    std::vector<unsigned char> heatmap_output(width * height * 3);
    
    #pragma omp parallel for schedule(dynamic)
    for (int j = 0; j < height; ++j) {
        for (int i = 0; i < width; ++i) {
            int index = j * width + i;
            int N = pixel_counts[index];
            if (N == 0) N = 1;

            glm::vec3 color = accum_buffer[index] / float(N);
            color = glm::sqrt(color); // Gamma 2.0
            color = glm::clamp(color, 0.0f, 1.0f);

            int out_idx = index * 3;
            image_output[out_idx + 0] = static_cast<unsigned char>(255.99f * color.r);
            image_output[out_idx + 1] = static_cast<unsigned char>(255.99f * color.g);
            image_output[out_idx + 2] = static_cast<unsigned char>(255.99f * color.b);

            float ratio = (float)N / current_spp; 
            ratio = std::clamp(ratio, 0.0f, 1.0f);
            
            heatmap_output[out_idx + 0] = static_cast<unsigned char>(255.99f * ratio);          // Red (More samples)
            heatmap_output[out_idx + 1] = static_cast<unsigned char>(255.99f * (1.0f - ratio)); // Green (Less samples)
            heatmap_output[out_idx + 2] = 0;
        }
    }

    std::string latest_img_name = generate_filename(SCENE_ID, false, method_tag, current_spp, true);
    std::string latest_heat_name = generate_filename(SCENE_ID, true, method_tag, current_spp, true);

    stbi_write_png(latest_img_name.c_str(), width, height, 3, image_output.data(), width * 3);
    stbi_write_png(latest_heat_name.c_str(), width, height, 3, heatmap_output.data(), width * 3);

    if (is_milestone) {
        std::string mile_img_name = generate_filename(SCENE_ID, false, method_tag, current_spp, false);
        std::string mile_heat_name = generate_filename(SCENE_ID, true, method_tag, current_spp, false);
        
        stbi_write_png(mile_img_name.c_str(), width, height, 3, image_output.data(), width * 3);
        stbi_write_png(mile_heat_name.c_str(), width, height, 3, heatmap_output.data(), width * 3);
        
        std::cout << " [Checkpoint Saved: " << mile_img_name << "]";
    }
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
            config.samples_per_pixel = 25600;
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
    std::string method_tag = config.use_photon_mapping ? "PM" : "PT";
    
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
    int next_save_milestone = config.samples_per_batch; 

    // --- RENDER LOOP (Batched) ---
    while (samples_loop_count < config.samples_per_pixel) {
        
        int start_active_count = total_active_pixels.load();
        if (start_active_count == 0) {
            std::cout << "\nAll pixels converged! Stopping early." << std::endl;
            break;
        }

        int current_batch_size = std::min(config.samples_per_batch, config.samples_per_pixel - samples_loop_count);
        
        std::atomic<int> processed_active_pixels{0};
        std::mutex print_mutex;

        // Parallelize pixel loops
        #pragma omp parallel for schedule(dynamic)
        for (int j = 0; j < height; ++j) {
            int row_processed_count = 0;

            for (int i = 0; i < width; ++i) {
                int index = j * width + i;

                if (config.use_adaptive_sampling && pixel_converged[index]) {
                    continue;
                }
                row_processed_count++;

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
            
            if (row_processed_count > 0) {
                int current_processed = (processed_active_pixels += row_processed_count);
                if (omp_get_thread_num() == 0) {
                     draw_progress_bar(current_processed, start_active_count, samples_loop_count + current_batch_size, total_active_pixels.load());
                }
            }
        }
        draw_progress_bar(start_active_count, start_active_count, samples_loop_count + current_batch_size, total_active_pixels.load());
        std::cout << std::flush;

        samples_loop_count += current_batch_size;

        bool is_milestone = false;
        if (samples_loop_count >= next_save_milestone) {
            is_milestone = true;
            next_save_milestone *= 2;
        }

        save_snapshot(samples_loop_count, width, height, accumulation_buffer, pixel_samples, method_tag, is_milestone);
        std::cout << std::flush;
    }

    save_snapshot(samples_loop_count, width, height, accumulation_buffer, pixel_samples, method_tag, true);

    std::cout << "\n\nRendering Complete!" << std::endl;
    return 0;
}