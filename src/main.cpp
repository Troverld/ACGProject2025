
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>

// External
#include <glm/glm.hpp>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// Project Headers
#include "core/utils.hpp"
#include "core/ray.hpp"
#include "scene/sphere.hpp"
#include "scene/scene.hpp"
#include "scene/camera.hpp"
#include "material/bsdf.hpp"
#include "renderer/integrator.hpp"

// --- Configuration ---
const int MAX_DEPTH = 50;
const int SAMPLES_PER_PIXEL = 50;
const int IMAGE_WIDTH = 800;
const float ASPECT_RATIO = 16.0f / 9.0f;

/**
 * @brief Constructs the scene.
 * Keeps main() clean.
 */
void setup_scene(Scene& world, Camera& cam) {
    // 1. Materials
    auto material_ground = std::make_shared<Lambertian>(glm::vec3(0.8f, 0.8f, 0.0f));
    auto material_center = std::make_shared<Lambertian>(glm::vec3(0.1f, 0.2f, 0.5f));
    auto material_left   = std::make_shared<Metal>(glm::vec3(0.8f, 0.8f, 0.8f), 0.3f);
    auto material_right  = std::make_shared<Metal>(glm::vec3(0.8f, 0.6f, 0.2f), 0.0f);

    // 2. Objects
    world.add(std::make_shared<Sphere>(glm::vec3( 0.0f, -100.5f, -1.0f), 100.0f, material_ground));
    world.add(std::make_shared<Sphere>(glm::vec3( 0.0f,    0.0f, -1.0f),   0.5f, material_center));
    world.add(std::make_shared<Sphere>(glm::vec3(-1.0f,    0.0f, -1.0f),   0.5f, material_left));
    world.add(std::make_shared<Sphere>(glm::vec3( 1.0f,    0.0f, -1.0f),   0.5f, material_right));

    // 3. Camera
    glm::vec3 lookfrom(0.0f, 0.0f, 1.0f);
    glm::vec3 lookat(0.0f, 0.0f, -1.0f);
    glm::vec3 vup(0.0f, 1.0f, 0.0f);
    float dist_to_focus = glm::length(lookfrom - lookat);
    float aperture = 0.0f; // Set to > 0.1 for Depth of Field effect

    cam = Camera(lookfrom, lookat, vup, 90.0f, ASPECT_RATIO, aperture, dist_to_focus);
}

int main() {
    // 1. Init
    const int height = static_cast<int>(IMAGE_WIDTH / ASPECT_RATIO);
    const int channels = 3;
    std::cout << "Rendering " << IMAGE_WIDTH << "x" << height << "..." << std::endl;

    Scene world;
    // Dummy init, will be overwritten by setup_scene
    Camera cam(glm::vec3(0), glm::vec3(0,0,-1), glm::vec3(0,1,0), 90, ASPECT_RATIO); 
    setup_scene(world, cam);

    // 2. Integrator
    PathIntegrator integrator(MAX_DEPTH);

    // 3. Render Loop
    std::vector<unsigned char> image(IMAGE_WIDTH * height * channels);

    for (int j = 0; j < height; ++j) {
        if (j % 10 == 0) std::cout << "Scanning line " << j << " remaining: " << height - j << std::endl;

        for (int i = 0; i < IMAGE_WIDTH; ++i) {
            glm::vec3 pixel_color(0.0f, 0.0f, 0.0f);

            for (int s = 0; s < SAMPLES_PER_PIXEL; ++s) {
                float u = (float(i) + random_float()) / (IMAGE_WIDTH - 1);
                float v = (float(height - 1 - j) + random_float()) / (height - 1);
                
                Ray r = cam.get_ray(u, v);
                pixel_color += integrator.estimate_radiance(r, world, MAX_DEPTH);
            }
            
            pixel_color /= float(SAMPLES_PER_PIXEL);

            // Gamma Correction (Gamma=2.0)
            pixel_color.r = sqrt(pixel_color.r);
            pixel_color.g = sqrt(pixel_color.g);
            pixel_color.b = sqrt(pixel_color.b);

            int index = (j * IMAGE_WIDTH + i) * channels;
            image[index + 0] = static_cast<unsigned char>(255.999f * std::clamp(pixel_color.r, 0.0f, 1.0f));
            image[index + 1] = static_cast<unsigned char>(255.999f * std::clamp(pixel_color.g, 0.0f, 1.0f));
            image[index + 2] = static_cast<unsigned char>(255.999f * std::clamp(pixel_color.b, 0.0f, 1.0f));
        }
    }

    stbi_write_png("phase3_output.png", IMAGE_WIDTH, height, channels, image.data(), IMAGE_WIDTH * channels);
    std::cout << "Done! Saved to phase3_output.png" << std::endl;

    return 0;
}