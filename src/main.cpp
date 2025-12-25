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

// --- Configuration ---
const int MAX_DEPTH = 10;
const int SAMPLES_PER_PIXEL = 1000; // Increase for high quality
const int SAMPLES_PER_BATCH = 100; // How often to save an image
const int IMAGE_WIDTH = 800;
const float ASPECT_RATIO = 16.0f / 9.0f;

// Photon Mapping Settings (Only used for Scene 2 usually)
const int NUM_PHOTON = 100000000; 
const float CAUSTIC_RADIUS = 0.3f;
const float GLOBAL_RADIUS = 0.4f;
const int K_NEAREST = 100;
const int FINAL_GATHER_BOUND = 4;

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
const int SCENE_ID = 7; 

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
    std::cout << "Saved snapshot: " << filename << std::flush; // Flush to ensure text appears immediately
}

int main() {
    const int height = static_cast<int>(IMAGE_WIDTH / ASPECT_RATIO); 
    
    std::cout << "Rendering Scene ID: " << SCENE_ID << " [" << IMAGE_WIDTH << "x" << height << "]" << std::endl;
    std::cout << "Target Samples: " << SAMPLES_PER_PIXEL << " (Batch Size: " << SAMPLES_PER_BATCH << ")" << std::endl;

    Scene world;
    Camera cam(glm::vec3(0), glm::vec3(0,0,-1), glm::vec3(0,1,0), 90, ASPECT_RATIO); 

    switch (SCENE_ID) {
        case 1: scene_materials_textures(world, cam, ASPECT_RATIO); break;
        case 2: scene_cornell_smoke_caustics(world, cam, ASPECT_RATIO); break;
        case 3: scene_motion_blur(world, cam, ASPECT_RATIO); break;
        case 4: scene_mesh_env(world, cam, ASPECT_RATIO); break;
        case 5: scene_5(world, cam, ASPECT_RATIO); break;
        case 6: scene_dispersion(world, cam, ASPECT_RATIO); break; // Added Scene 6
        case 7: scene_prism_spectrum(world, cam, ASPECT_RATIO); break; // Added
        default: scene_materials_textures(world, cam, ASPECT_RATIO); break;
    }

    world.build_bvh(0.0f, 1.0f); 

    std::unique_ptr<Integrator> integrator;

    if (SCENE_ID == 2 || SCENE_ID ==7) {
        std::cout << "Using Photon Integrator..." << std::endl;
        integrator = std::make_unique<PhotonIntegrator>(
            MAX_DEPTH, NUM_PHOTON, CAUSTIC_RADIUS, GLOBAL_RADIUS, K_NEAREST, FINAL_GATHER_BOUND, 0.0f, 1.0f, world
        );
    } else {
        std::cout << "Using Path Integrator (MIS + NEE)..." << std::endl;
        integrator = std::make_unique<PathIntegrator>(MAX_DEPTH, world);
    }

    // --- ACCUMULATION BUFFER ---
    // Stores high precision sum of all samples
    std::vector<glm::vec3> accumulation_buffer(IMAGE_WIDTH * height, glm::vec3(0.0f));

    int samples_so_far = 0;

    // --- RENDER LOOP (Batched) ---
    while (samples_so_far < SAMPLES_PER_PIXEL) {
        
        // Determine samples for this batch (handle remainder)
        int current_batch_size = std::min(SAMPLES_PER_BATCH, SAMPLES_PER_PIXEL - samples_so_far);
        
        std::cout << "\nRendering Batch: samples " << samples_so_far + 1 
                  << " to " << samples_so_far + current_batch_size << "..." << std::endl;

        // Parallelize pixel loops
        #pragma omp parallel for schedule(dynamic)
        for (int j = 0; j < height; ++j) {
            
            // Only print progress for one thread to avoid clutter
            if (omp_get_thread_num() == 0 && j % 20 == 0) {
                 // Simple spinner or progress indicator
            }

            for (int i = 0; i < IMAGE_WIDTH; ++i) {
                glm::vec3 batch_color(0.0f);

                // Run the samples for this batch
                for (int s = 0; s < current_batch_size; ++s) {
                    float u = (float(i) + random_float()) / IMAGE_WIDTH;
                    float v = (float(height - 1 - j) + random_float()) / height;

                    Ray r = cam.get_ray(u, v);
                    batch_color += integrator->estimate_radiance(r, world);
                }

                // Add batch result to accumulation buffer
                // Note: No race condition here because each thread owns specific (i, j)
                int index = j * IMAGE_WIDTH + i;
                accumulation_buffer[index] += batch_color;
            }
        }

        samples_so_far += current_batch_size;

        // Save image after every batch
        save_snapshot(samples_so_far, IMAGE_WIDTH, height, accumulation_buffer);
    }

    std::cout << "\n\nRendering Complete!" << std::endl;
    return 0;
}