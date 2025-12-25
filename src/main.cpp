#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <omp.h>
#include <iomanip>
#include <sstream>

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
    int samples_per_batch; // How often to save
    int max_depth;

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
        800, 16.0f/9.0f,    // width, aspect
        1000, 100, 10,      // samples, batch, depth
        false,              // use_photon_mapping
        5000000, 0.1f, 0.4f, 200, 4 // default photon settings
    };
}

// ==========================
// SCENE SELECTION
// 1: Materials & Textures (DoF, Normal Map)
// 2: Cornell Box (Volumetrics, Caustics, PhotonMap)
// 3: Motion Blur (BVH stress test)
// 4: Mesh & Env Map
// 5: Scene 5
// 6: Chromatic Dispersion (New)
// 7: Prism Spectrum (New)
// ==========================
const int SCENE_ID = 2;

void save_snapshot(int current_samples, int width, int height, const std::vector<glm::vec3>& accum_buffer) {
    std::vector<unsigned char> image_output(width * height * 3);
    
    // We don't parallelize this part usually because I/O is the bottleneck, 
    // but conversion can be parallelized.
    #pragma omp parallel for schedule(dynamic)
    for (int j = 0; j < height; ++j) {
        for (int i = 0; i < width; ++i) {
            int index = j * width + i;
            
            // Average the accumulated color
            glm::vec3 color = accum_buffer[index] / float(current_samples);

            // Gamma Correction (Gamma 2.0 approximation)
            color = glm::sqrt(color);

            // Clamp and convert
            int out_idx = index * 3;
            image_output[out_idx + 0] = static_cast<unsigned char>(255.999f * std::clamp(color.r, 0.0f, 1.0f));
            image_output[out_idx + 1] = static_cast<unsigned char>(255.999f * std::clamp(color.g, 0.0f, 1.0f));
            image_output[out_idx + 2] = static_cast<unsigned char>(255.999f * std::clamp(color.b, 0.0f, 1.0f));
        }
    }

    // Generate filename with zero padding: output_scene_2_samples_0050.png
    std::stringstream ss;
    ss << "output_scene_" << SCENE_ID << "_samples_" << std::setw(4) << std::setfill('0') << current_samples << ".png";
    std::string filename = ss.str();

    stbi_write_png(filename.c_str(), width, height, 3, image_output.data(), width * 3);
    // std::cout << "Saved snapshot: " << filename << std::flush; // Flush to ensure text appears immediately
    printf("Saved snapshot: %s\r", filename.c_str());
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
            config.num_photons = 5000000;
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
            config.num_photons = 100000000;
            config.caustic_radius = 0.3f;
            config.global_radius = 0.4f;
            config.k_nearest = 100;
            config.final_gather_bound = 5;
            scene_prism_spectrum(world, cam, config.aspect_ratio);
            break;
        default: scene_materials_textures(world, cam, config.aspect_ratio); break;
    }

    const int width = config.width; 
    const int height = static_cast<int>(width / config.aspect_ratio); 
    
    std::cout << "Rendering Scene ID: " << SCENE_ID << " [" << width << "x" << height << "]" << std::endl;
    std::cout << "Target Samples: " << config.samples_per_pixel << " (Batch Size: " << config.samples_per_batch << ")" << std::endl;

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

    // --- ACCUMULATION BUFFER ---
    // Stores high precision sum of all samples
    std::vector<glm::vec3> accumulation_buffer(width * height, glm::vec3(0.0f));

    int samples_so_far = 0;

    // --- RENDER LOOP (Batched) ---
    while (samples_so_far < config.samples_per_pixel) {
        
        // Determine samples for this batch (handle remainder)
        int current_batch_size = std::min(config.samples_per_batch, config.samples_per_pixel - samples_so_far);

        // Parallelize pixel loops
        #pragma omp parallel for schedule(dynamic)
        for (int j = 0; j < height; ++j) {
            
            // Only print progress for one thread to avoid clutter
            if (omp_get_thread_num() == 0 && j % 20 == 0) {
                 // Simple spinner or progress indicator
            }

            for (int i = 0; i < width; ++i) {
                glm::vec3 batch_color(0.0f);

                // Run the samples for this batch
                for (int s = 0; s < current_batch_size; ++s) {
                    float u = (float(i) + random_float()) / width;
                    float v = (float(height - 1 - j) + random_float()) / height;

                    Ray r = cam.get_ray(u, v);
                    batch_color += integrator->estimate_radiance(r, world);
                }

                // Add batch result to accumulation buffer
                // Note: No race condition here because each thread owns specific (i, j)
                int index = j * width + i;
                accumulation_buffer[index] += batch_color;
            }
        }

        samples_so_far += current_batch_size;

        // Save image after every batch
        save_snapshot(samples_so_far, width, height, accumulation_buffer);
    }

    std::cout << "\n\nRendering Complete!" << std::endl;
    return 0;
}