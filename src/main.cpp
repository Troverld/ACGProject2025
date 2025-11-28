#include <iostream>
#include <vector>
#include <string>
#include <memory>

// External
#include <glm/glm.hpp>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// Project Headers
#include "core/utils.hpp"
#include "core/ray.hpp"
#include "scene/object.hpp"
#include "scene/sphere.hpp"
#include "scene/scene.hpp"
#include "scene/camera.hpp"
#include "material/material.hpp"
#include "material/bsdf.hpp"

// 递归最大深度
const int MAX_DEPTH = 50;

/**
 * @brief Core Path Tracing function (Recursive).
 * Calculates the radiance along a ray.
 * 
 * @param r The current ray.
 * @param world The scene containing all objects.
 * @param depth Current recursion depth (decrements each bounce).
 * @return glm::vec3 The calculated color (radiance).
 */
glm::vec3 ray_color(const Ray& r, const Object& world, int depth) {
    HitRecord rec;

    // If we've exceeded the ray bounce limit, no more light is gathered.
    if (depth <= 0)
        return glm::vec3(0, 0, 0);

    // t_min = SHADOW_EPSILON to avoid shadow acne
    if (world.intersect(r, SHADOW_EPSILON, Infinity, rec)) {
        Ray scattered;
        glm::vec3 attenuation;
        
        // Ask the material how to scatter the light
        if (rec.mat_ptr->scatter(r, rec, attenuation, scattered)) {
            // Recursive step: color = attenuation * ray_color(scattered_ray)
            return attenuation * ray_color(scattered, world, depth - 1);
        }
        // Absorbed (e.g., hit a black body or internal reflection limit)
        return glm::vec3(0, 0, 0);
    }

    // Background (Skybox)
    glm::vec3 unit_direction = glm::normalize(r.direction());
    float t = 0.5f * (unit_direction.y + 1.0f);
    return (1.0f - t) * glm::vec3(1.0f, 1.0f, 1.0f) + t * glm::vec3(0.5f, 0.7f, 1.0f);
}

int main() {
    // 1. Image Settings
    const float aspect_ratio = 16.0f / 9.0f;
    const int width = 800;
    const int height = static_cast<int>(width / aspect_ratio);
    const int channels = 3;
    const int samples_per_pixel = 50; // AA samples (increase for better quality)

    std::cout << "Rendering " << width << "x" << height << " with " << samples_per_pixel << " spp..." << std::endl;

    // 2. Scene Setup
    Scene world;

    // Materials
    auto material_ground = std::make_shared<Lambertian>(glm::vec3(0.8f, 0.8f, 0.0f));
    auto material_center = std::make_shared<Lambertian>(glm::vec3(0.1f, 0.2f, 0.5f));
    auto material_left   = std::make_shared<Metal>(glm::vec3(0.8f, 0.8f, 0.8f), 0.3f); // Fuzzy metal
    auto material_right  = std::make_shared<Metal>(glm::vec3(0.8f, 0.6f, 0.2f), 0.0f); // Polished gold

    // Objects
    world.add(std::make_shared<Sphere>(glm::vec3( 0.0f, -100.5f, -1.0f), 100.0f, material_ground));
    world.add(std::make_shared<Sphere>(glm::vec3( 0.0f,    0.0f, -1.0f),   0.5f, material_center));
    world.add(std::make_shared<Sphere>(glm::vec3(-1.0f,    0.0f, -1.0f),   0.5f, material_left));
    world.add(std::make_shared<Sphere>(glm::vec3( 1.0f,    0.0f, -1.0f),   0.5f, material_right));

    // 3. Camera Setup
    glm::vec3 lookfrom(0.0f, 0.0f, 1.0f); // Move camera back slightly
    glm::vec3 lookat(0.0f, 0.0f, -1.0f);
    glm::vec3 vup(0.0f, 1.0f, 0.0f);
    Camera cam(lookfrom, lookat, vup, 90.0f, aspect_ratio);

    // 4. Rendering Loop
    std::vector<unsigned char> image(width * height * channels);

    for (int j = 0; j < height; ++j) {
        if (j % 10 == 0) std::cout << "Scanning line " << j << " remaining: " << height - j << std::endl;

        for (int i = 0; i < width; ++i) {
            glm::vec3 pixel_color(0.0f, 0.0f, 0.0f);

            // Anti-Aliasing Loop
            for (int s = 0; s < samples_per_pixel; ++s) {
                // Random offset within the pixel [i, i+1] x [j, j+1]
                float u = (float(i) + random_float()) / (width - 1);
                float v = (float(height - 1 - j) + random_float()) / (height - 1);
                
                Ray r = cam.get_ray(u, v);
                pixel_color += ray_color(r, world, MAX_DEPTH);
            }
            
            // Average the color
            pixel_color /= float(samples_per_pixel);

            // Gamma Correction (Approximate Gamma=2.0 by taking sqrt)
            pixel_color.r = sqrt(pixel_color.r);
            pixel_color.g = sqrt(pixel_color.g);
            pixel_color.b = sqrt(pixel_color.b);

            // Clamp and write
            int index = (j * width + i) * channels;
            image[index + 0] = static_cast<unsigned char>(255.999f * std::clamp(pixel_color.r, 0.0f, 1.0f));
            image[index + 1] = static_cast<unsigned char>(255.999f * std::clamp(pixel_color.g, 0.0f, 1.0f));
            image[index + 2] = static_cast<unsigned char>(255.999f * std::clamp(pixel_color.b, 0.0f, 1.0f));
        }
    }

    // 5. Output
    stbi_write_png("phase3_output.png", width, height, channels, image.data(), width * channels);
    std::cout << "Done! Saved to phase3_output.png" << std::endl;

    return 0;
}