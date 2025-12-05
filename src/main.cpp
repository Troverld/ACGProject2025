#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <omp.h>

// External
#include <glm/glm.hpp>
#include "stb_image_write.h"
#include "stb_image.h"

// Project Headers
#include "core/utils.hpp"
#include "core/ray.hpp"
#include "scene/scene.hpp"
#include "scene/camera.hpp"
#include "renderer/integrator.hpp"
#include "renderer/photon_integrator.hpp"

// Scene List
#include "scene_list.cpp"

// --- Configuration ---
const int MAX_DEPTH = 10;
const int SAMPLES_PER_PIXEL = 250; // Increase for high quality
const int IMAGE_WIDTH = 800;
const float ASPECT_RATIO = 16.0f / 9.0f;

// Photon Mapping Settings (Only used for Scene 2 usually)
const int NUM_PHOTON = 5000000; 
const float CAUSTIC_RADIUS = 1.0f;
const float GLOBAL_RADIUS = 3.0f;
const int FINAL_GATHER_BOUND = 3;

// ==========================
// SCENE SELECTION
// 1: Materials & Textures (DoF, Normal Map)
// 2: Cornell Box (Volumetrics, Caustics, PhotonMap)
// 3: Motion Blur (BVH stress test)
// 4: Mesh & Env Map
// ==========================
const int SCENE_ID = 3; 

int main() {
    const int height = static_cast<int>(IMAGE_WIDTH / ASPECT_RATIO); 
    const int channels = 3;
    
    std::cout << "Rendering Scene ID: " << SCENE_ID << " [" << IMAGE_WIDTH << "x" << height << "]" << std::endl;

    Scene world;
    Camera cam(glm::vec3(0), glm::vec3(0,0,-1), glm::vec3(0,1,0), 90, ASPECT_RATIO); 

    // Scene Setup Switch
    switch (SCENE_ID) {
        case 1: scene_materials_textures(world, cam, ASPECT_RATIO); break;
        case 2: scene_cornell_smoke_caustics(world, cam, ASPECT_RATIO); break;
        case 3: scene_motion_blur(world, cam, ASPECT_RATIO); break;
        case 4: scene_mesh_env(world, cam, ASPECT_RATIO); break;
        default: scene_materials_textures(world, cam, ASPECT_RATIO); break;
    }

    // Build Acceleration Structure
    // Note: Motion blur scene requires time range [0, 1]
    world.build_bvh(0.0f, 1.0f); 

    // Integrator Selection
    std::unique_ptr<Integrator> integrator;

    if (SCENE_ID == 0) {
        std::cout << "Using Photon Integrator..." << std::endl;
        integrator = std::make_unique<PhotonIntegrator>(
            MAX_DEPTH, NUM_PHOTON, CAUSTIC_RADIUS, GLOBAL_RADIUS, FINAL_GATHER_BOUND, world
        );
    } else {
        std::cout << "Using Path Integrator (MIS + NEE)..." << std::endl;
        integrator = std::make_unique<PathIntegrator>(MAX_DEPTH);
    }

    // Render Loop
    std::vector<unsigned char> image(IMAGE_WIDTH * height * channels);

    #pragma omp parallel for schedule(dynamic)
    for (int j = 0; j < height; ++j) {
        if (j % 10 == 0) 
            printf("Scanning line %d / %d \r", j, height);

        for (int i = 0; i < IMAGE_WIDTH; ++i) {
            glm::vec3 pixel_color(0.0f, 0.0f, 0.0f);

            for (int s = 0; s < SAMPLES_PER_PIXEL; ++s) {
                float u = (float(i) + random_float()) / IMAGE_WIDTH;
                float v = (float(height - 1 - j) + random_float()) / height;

                Ray r = cam.get_ray(u, v);
                pixel_color += integrator->estimate_radiance(r, world);
            }
            
            pixel_color /= float(SAMPLES_PER_PIXEL);

            // Gamma Correction
            pixel_color = glm::sqrt(pixel_color);

            int index = (j * IMAGE_WIDTH + i) * channels;
            image[index + 0] = static_cast<unsigned char>(255.999f * std::clamp(pixel_color.r, 0.0f, 1.0f));
            image[index + 1] = static_cast<unsigned char>(255.999f * std::clamp(pixel_color.g, 0.0f, 1.0f));
            image[index + 2] = static_cast<unsigned char>(255.999f * std::clamp(pixel_color.b, 0.0f, 1.0f));
        }
    }

    std::string filename = "output_scene_" + std::to_string(SCENE_ID) + ".png";
    stbi_write_png(filename.c_str(), IMAGE_WIDTH, height, channels, image.data(), IMAGE_WIDTH * channels);
    std::cout << "\nDone! Saved to " << filename << std::endl;

    return 0;
}