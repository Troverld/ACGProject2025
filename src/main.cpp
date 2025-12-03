#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>

// External
#include <glm/glm.hpp>
#include "stb_image_write.h"
#include "stb_image.h"

// Project Headers
#include "core/utils.hpp"
#include "core/ray.hpp"
#include "scene/sphere.hpp"
#include "scene/triangle.hpp"
#include "scene/scene.hpp"
#include "scene/camera.hpp"
#include "scene/volume.hpp"
#include "material/bsdf.hpp"
#include "texture/checker.hpp" // Include checker
#include "texture/perlin.hpp"  // Include perlin
#include "texture/image_texture.hpp" 
#include "renderer/integrator.hpp"

// --- Configuration ---
const int MAX_DEPTH = 50;
const int SAMPLES_PER_PIXEL = 100;
const int IMAGE_WIDTH = 800;
const float ASPECT_RATIO = 1.0f;
// const float ASPECT_RATIO = 16.0f / 9.0f;

/**
 * @brief Constructs Cornell box and two smoke block.
 * Demonstrates volumetric.
 */
void setup_cornell_box_smoke(Scene& world, Camera& cam) {
    world.clear();

    // Materials
    auto red   = std::make_shared<Lambertian>(glm::vec3(0.65f, 0.05f, 0.05f));
    auto white = std::make_shared<Lambertian>(glm::vec3(0.73f, 0.73f, 0.73f));
    auto green = std::make_shared<Lambertian>(glm::vec3(0.12f, 0.45f, 0.15f));
    auto light = std::make_shared<DiffuseLight>(glm::vec3(15.0f, 15.0f, 15.0f));

    // Cornell Box Walls
    // Floor
    world.add(std::make_shared<Triangle>(glm::vec3(555,0,0), glm::vec3(0,0,0), glm::vec3(0,0,555), white));
    world.add(std::make_shared<Triangle>(glm::vec3(555,0,555), glm::vec3(555,0,0), glm::vec3(0,0,555), white));
    
    // Ceiling
    world.add(std::make_shared<Triangle>(glm::vec3(555,555,555), glm::vec3(0,555,555), glm::vec3(0,555,0), white));
    world.add(std::make_shared<Triangle>(glm::vec3(0,555,0), glm::vec3(555,555,0), glm::vec3(555,555,555), white));

    // Back Wall
    world.add(std::make_shared<Triangle>(glm::vec3(555,0,555), glm::vec3(0,0,555), glm::vec3(0,555,555), white));
    world.add(std::make_shared<Triangle>(glm::vec3(0,555,555), glm::vec3(555,555,555), glm::vec3(555,0,555), white));

    // Left Wall (Green)
    world.add(std::make_shared<Triangle>(glm::vec3(555,0,555), glm::vec3(555,555,555), glm::vec3(555,555,0), green));
    world.add(std::make_shared<Triangle>(glm::vec3(555,555,0), glm::vec3(555,0,0), glm::vec3(555,0,555), green));

    // Right Wall (Red)
    world.add(std::make_shared<Triangle>(glm::vec3(0,0,555), glm::vec3(0,555,0), glm::vec3(0,555,555), red));
    world.add(std::make_shared<Triangle>(glm::vec3(0,555,0), glm::vec3(0,0,555), glm::vec3(0,0,0), red));

    // Light (Small square at top)
    glm::vec3 l0(213, 554, 227);
    glm::vec3 l1(343, 554, 227);
    glm::vec3 l2(343, 554, 332);
    glm::vec3 l3(213, 554, 332);
    world.add(std::make_shared<Triangle>(l0, l1, l2, light));
    world.add(std::make_shared<Triangle>(l0, l2, l3, light));

    // --- Smoke / Fog Objects ---
    
    // Define bounds using Spheres for simplicity (or implement Box class later)
    auto box1_boundary = std::make_shared<Sphere>(glm::vec3(130, 165, 65), 65.0f, white); // Placeholder geometry
    auto box2_boundary = std::make_shared<Sphere>(glm::vec3(265, 165, 295), 80.0f, white);

    // Create Volumetric Objects
    // 0.01 density smoke (White)
    world.add(std::make_shared<ConstantMedium>(box1_boundary, 0.1f, glm::vec3(1.0f, 1.0f, 1.0f)));
    
    // 0.01 density smoke (Black/Dark) - absorbs light
    world.add(std::make_shared<ConstantMedium>(box2_boundary, 0.1f, glm::vec3(0.0f, 0.0f, 0.0f)));

    // Background
    world.set_background(std::make_shared<SolidColor>(0.2f, 0.2f, 0.2f));

    // Camera setup for Cornell Box
    glm::vec3 lookfrom(278, 278, -800);
    glm::vec3 lookat(278, 278, 0);
    glm::vec3 vup(0, 1, 0);
    float dist_to_focus = 10.0f;
    float aperture = 0.0f;
    float vfov = 40.0f;

    cam = Camera(lookfrom, lookat, vup, vfov, ASPECT_RATIO, aperture, dist_to_focus);
}

/**
 * @brief Constructs the scene.
 * Demonstrates various textures.
 */
void setup_scene(Scene& world, Camera& cam) {
    world.clear();

    // 1. Materials
    auto tex_checker = std::make_shared<CheckerTexture>(
        glm::vec3(0.2f, 0.3f, 0.1f), 
        glm::vec3(0.9f, 0.9f, 0.9f),
        1.0f // Scale for procedural, but for triangles we use UVs
    );
    auto mat_ground   = std::make_shared<Lambertian>(glm::vec3(0.5f, 0.5f, 0.5f));
    auto mat_glass    = std::make_shared<Dielectric>(glm::vec3(1.0f), 1.5f);
    // auto mat_matte    = std::make_shared<Lambertian>(glm::vec3(0.4f, 0.2f, 0.1f)); // Reddish

    auto mat_bricks = std::make_shared<Lambertian>(std::make_shared<ImageTexture>("assets/texture/red_brick/red_brick_diff_1k.png"));
    auto normal_tex = std::make_shared<ImageTexture>("assets/texture/red_brick/red_brick_nor_gl_1k.png");
    mat_bricks->set_normal_map(normal_tex);
    auto mat_metal = std::make_shared<Metal>(glm::vec3(0.7f, 0.6f, 0.5f), 0.1f); // Rough Gold
    auto mat_checker = std::make_shared<Lambertian>(tex_checker); // Tetrahedron Material (Blueish Lambertian)

    // High intensity light (tests NEE dominant case)
    auto mat_light_strong = std::make_shared<DiffuseLight>(glm::vec3(15.0f, 15.0f, 15.0f)); 
    // Low intensity large light (tests BSDF sampling dominant case)
    auto mat_light_dim    = std::make_shared<DiffuseLight>(glm::vec3(2.0f, 2.0f, 4.0f));    

    // 2. Objects
    
    // Ground (Huge sphere to approximate a plane)
    // world.add(std::make_shared<Sphere>(glm::vec3(0.0f, -1000.0f, 0.0f), 1000.0f, mat_ground));
    glm::vec3 v0(-20.0f, 0.0f, -20.0f);
    glm::vec3 v1( 20.0f, 0.0f, -20.0f);
    glm::vec3 v2( 20.0f, 0.0f,  20.0f);
    glm::vec3 v3(-20.0f, 0.0f,  20.0f);

    float uv_scale = 10.0f;
    glm::vec2 t0(0, 0);
    glm::vec2 t1(uv_scale, 0);
    glm::vec2 t2(uv_scale, uv_scale);
    glm::vec2 t3(0, uv_scale);

    // Triangle 1 (v0, v1, v2)
    world.add(std::make_shared<Triangle>(v0, v1, v2, mat_ground, t0, t1, t2));
    // Triangle 2 (v0, v2, v3)
    world.add(std::make_shared<Triangle>(v0, v2, v3, mat_ground, t0, t2, t3));

     // --- Tetrahedron ---
    // Center at (-1.5, 0.5, 1.0), Radius ~1
    glm::vec3 p0(-1.2f, 0.0f, 2.5f); // Base 1
    glm::vec3 p1(-0.2f, 0.0f, 1.5f); // Base 2
    glm::vec3 p2(-2.2f, 0.0f, 1.5f); // Base 3
    glm::vec3 p3(-1.2f, 1.4f, 1.9f); // Top

    // Base Triangle (Order for normal pointing down/out)
    world.add(std::make_shared<Triangle>(p2, p1, p0, mat_bricks)); 
    // Side 1
    world.add(std::make_shared<Triangle>(p0, p1, p3, mat_bricks));
    // Side 2
    world.add(std::make_shared<Triangle>(p1, p2, p3, mat_bricks));
    // Side 3
    world.add(std::make_shared<Triangle>(p2, p0, p3, mat_bricks));

    // Three main subjects
    world.add(std::make_shared<Sphere>(glm::vec3(-2.2f, 1.0f, 0.0f), 1.0f, mat_glass));
    world.add(std::make_shared<Sphere>(glm::vec3( 0.0f, 1.0f, 0.0f), 1.0f, mat_checker));
    world.add(std::make_shared<Sphere>(glm::vec3( 2.2f, 1.0f, 0.0f), 1.0f, mat_metal));

    // Lights
    // Small strong light above
    world.add(std::make_shared<Sphere>(glm::vec3(0.0f, 5.0f, 0.0f), 0.5f, mat_light_strong));
    // Small strong light front
    world.add(std::make_shared<Sphere>(glm::vec3(4.0f, 3.0f, 4.0f), 0.7f, mat_light_dim));
    // Large dim light to the side/back
    world.add(std::make_shared<Sphere>(glm::vec3(-4.0f, 3.0f, -3.0f), 1.5f, mat_light_dim));

    // Background (Black to verify lighting strictly comes from emissive objects)
    world.set_background(std::make_shared<SolidColor>(0.2f, 0.2f, 0.2f));

    // 3. Camera Setup
    glm::vec3 lookfrom(0.0f, 4.0f, 8.0f);
    glm::vec3 lookat(0.0f, 1.0f, 0.0f);
    glm::vec3 vup(0.0f, 1.0f, 0.0f);
    float dist_to_focus = glm::length(lookfrom - lookat);
    float aperture = 0.05f; 

    // FOV 30 for a portrait-like view
    cam = Camera(lookfrom, lookat, vup, 30.0f, ASPECT_RATIO, aperture, dist_to_focus);
}

int main() {
    // 1. Init
    // Reduced resolution for quick verification, increase for final render
    const int height = static_cast<int>(IMAGE_WIDTH / ASPECT_RATIO); 
    const int channels = 3;
    
    std::cout << "Rendering " << IMAGE_WIDTH << "x" << height << "..." << std::endl;
    std::cout << "Samples: " << SAMPLES_PER_PIXEL << ", Max Depth: " << MAX_DEPTH << std::endl;

    Scene world;
    Camera cam(glm::vec3(0), glm::vec3(0,0,-1), glm::vec3(0,1,0), 90, ASPECT_RATIO); 
    // setup_scene(world, cam);
    setup_cornell_box_smoke(world, cam);

    world.build_bvh(0.0f, 1.0f); 

    // 2. Integrator
    PathIntegrator integrator(MAX_DEPTH);

    // 3. Render Loop
    std::vector<unsigned char> image(IMAGE_WIDTH * height * channels);

    #pragma omp parallel for schedule(guided)
    for (int j = 0; j < height; ++j) {
        if (j % 10 == 0) std::cout << "Scanning line " << j << " remaining: " << height - j << std::endl;

        for (int i = 0; i < IMAGE_WIDTH; ++i) {
            glm::vec3 pixel_color(0.0f, 0.0f, 0.0f);

            for (int s = 0; s < SAMPLES_PER_PIXEL; ++s) {
                float u = (float(i) + random_float()) / IMAGE_WIDTH;
                float v = (float(height - 1 - j) + random_float()) / height;

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

    stbi_write_png("cornell_box_smoke.png", IMAGE_WIDTH, height, channels, image.data(), IMAGE_WIDTH * channels);
    std::cout << "Done! Saved to cornell_box_smoke.png" << std::endl;

    return 0;
}