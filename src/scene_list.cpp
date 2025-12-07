#pragma once

#include "scene/scene.hpp"
#include "scene/camera.hpp"
#include "object/object_agg.hpp"
#include "material/material_agg.hpp"
#include "texture/texture_agg.hpp"

// =======================================================================
// Scene 1: Advanced Materials & Textures
// 验证功能: 
// 1. Texture Mapping (Checker, Image, Perlin)
// 2. Normal Mapping (Bump map)
// 3. Material (Glass, Metal, Matte)
// 4. Depth of Field (Aperture > 0)
// =======================================================================
void scene_materials_textures(Scene& world, Camera& cam, float aspect) {
    world.clear();

    // Textures
    auto checker = std::make_shared<CheckerTexture>(glm::vec3(0.2f, 0.3f, 0.1f), glm::vec3(0.9f), 10.0f);
    auto perlin  = std::make_shared<Perlin>(4.0f);
    
    // Materials
    auto mat_ground = std::make_shared<Lambertian>(checker);
    auto mat_glass  = std::make_shared<Dielectric>(glm::vec3(1.0f), 1.5f);
    auto mat_metal  = std::make_shared<Metal>(glm::vec3(0.7f, 0.6f, 0.5f), 0.05f);
    auto mat_noise  = std::make_shared<Lambertian>(perlin);
    
    // Ground
    world.add(std::make_shared<Sphere>(glm::vec3(0.0f, -1000.0f, 0.0f), 1000.0f, mat_ground));

    // Objects
    world.add(std::make_shared<Sphere>(glm::vec3(0.0f, 1.0f, 0.0f), 1.0f, mat_glass));      // Center: Glass
    world.add(std::make_shared<Sphere>(glm::vec3(-4.0f, 1.0f, 0.0f), 1.0f, mat_noise));     // Left: Perlin Noise
    world.add(std::make_shared<Sphere>(glm::vec3(4.0f, 1.0f, 0.0f), 1.0f, mat_metal));      // Right: Metal

    // Normal Map Test (Front)
    // 需要确保 assets/texture/red_brick/ 路径下有纹理，否则用纯色替代
    // 这里假设你有图片，如果没有，会自动回退或报错，可根据实际情况注释掉
    auto diff_tex = std::make_shared<ImageTexture>("assets/texture/red_brick/red_brick_diff_1k.png");
    auto norm_tex = std::make_shared<ImageTexture>("assets/texture/red_brick/red_brick_nor_gl_1k.png");
    auto mat_brick = std::make_shared<Lambertian>(diff_tex);
    mat_brick->set_normal_map(norm_tex);
    world.add(std::make_shared<Sphere>(glm::vec3(0, 0.5, 3), 0.5f, mat_brick)); 

    // Camera
    glm::vec3 lookfrom(13, 2, 3);
    glm::vec3 lookat(0, 0, 0);
    float dist_to_focus = 10.0f;
    float aperture = 0.1f; // Enable Depth of Field

    cam = Camera(lookfrom, lookat, glm::vec3(0,1,0), 20.0f, aspect, aperture, dist_to_focus);
    
    // Environment Lighting (Background)
    world.set_background(std::make_shared<SolidColor>(0.7f, 0.8f, 1.0f));
}

// =======================================================================
// Scene 2: Volumetrics, Caustics & Photon Mapping
// 验证功能:
// 1. Photon Mapping (Glass sphere focusing light)
// 2. Volumetric Rendering (Homogeneous fog)
// 3. Area Light (Quad light)
// =======================================================================
void scene_cornell_smoke_caustics(Scene& world, Camera& cam, float aspect) {
    world.clear();

    auto red   = std::make_shared<Lambertian>(glm::vec3(0.65f, 0.05f, 0.05f));
    auto white = std::make_shared<Lambertian>(glm::vec3(0.73f, 0.73f, 0.73f));
    auto green = std::make_shared<Lambertian>(glm::vec3(0.12f, 0.45f, 0.15f));
    auto light = std::make_shared<DiffuseLight>(glm::vec3(15.0f));
    auto glass = std::make_shared<Dielectric>(glm::vec3(1.0f), 1.5f);

    // Cornell Box Walls
    world.add(std::make_shared<Triangle>(glm::vec3(555.0f,0.0f,0.0f), glm::vec3(0.0f,0.0f,0.0f), glm::vec3(0.0f,0.0f,555.0f), white)); // Floor 1
    world.add(std::make_shared<Triangle>(glm::vec3(555.0f,0.0f,555.0f), glm::vec3(555.0f,0.0f,0.0f), glm::vec3(0.0f,0.0f,555.0f), white)); // Floor 2
    world.add(std::make_shared<Triangle>(glm::vec3(555.0f,555.0f,555.0f), glm::vec3(0.0f,555.0f,555.0f), glm::vec3(0.0f,555.0f,0.0f), white)); // Ceiling
    world.add(std::make_shared<Triangle>(glm::vec3(0.0f,555.0f,0.0f), glm::vec3(555.0f,555.0f,0.0f), glm::vec3(555.0f,555.0f,555.0f), white)); // Ceiling
    world.add(std::make_shared<Triangle>(glm::vec3(555.0f,0.0f,555.0f), glm::vec3(0.0f,0.0f,555.0f), glm::vec3(0.0f,555.0f,555.0f), white)); // Back
    world.add(std::make_shared<Triangle>(glm::vec3(0.0f,555.0f,555.0f), glm::vec3(555.0f,555.0f,555.0f), glm::vec3(555.0f,0.0f,555.0f), white)); // Back
    world.add(std::make_shared<Triangle>(glm::vec3(555.0f,0.0f,555.0f), glm::vec3(555.0f,555.0f,555.0f), glm::vec3(555.0f,555.0f,0.0f), green)); // Left
    world.add(std::make_shared<Triangle>(glm::vec3(555.0f,555.0f,0.0f), glm::vec3(555.0f,0.0f,0.0f), glm::vec3(555.0f,0.0f,555.0f), green)); // Left
    world.add(std::make_shared<Triangle>(glm::vec3(0.0f,0.0f,555.0f), glm::vec3(0.0f,555.0f,0.0f), glm::vec3(0.0f,555.0f,555.0f), red)); // Right
    world.add(std::make_shared<Triangle>(glm::vec3(0.0f,555.0f,0.0f), glm::vec3(0.0f,0.0f,555.0f), glm::vec3(0.0f,0.0f,0.0f), red)); // Right

    // Light (Small area light on ceiling)
    world.add(std::make_shared<Triangle>(glm::vec3(213.0f,554.0f,227.0f), glm::vec3(343.0f,554.0f,227.0f), glm::vec3(343.0f,554.0f,332.0f), light));
    world.add(std::make_shared<Triangle>(glm::vec3(213.0f,554.0f,227.0f), glm::vec3(343.0f,554.0f,332.0f), glm::vec3(213.0f,554.0f,332.0f), light));

    world.add_light(std::make_shared<PointLight>(glm::vec3(120.0f,239.0f,127.0f), glm::vec3(4e3f, 8e3f, 1e3f)));

    // Glass Sphere for Caustics
    world.add(std::make_shared<Sphere>(glm::vec3(190.0f, 90.0f, 190.0f), 90.0f, glass));

    // Fog / Smoke Block
    auto boundary_fog = std::make_shared<Sphere>(glm::vec3(360.0f, 150.0f, 360.0f), 80.0f, white); // Using Sphere as boundary for simplicity
    auto boundary_fireball = std::make_shared<Sphere>(glm::vec3(410.0f, 200.0f, 250.0f), 80.0f, white); // Using Sphere as boundary for simplicity
    
    // Mesh (Bunny)
    auto boundary_bunny = std::make_shared<Mesh>(
        "assets/model/bunny_200_subdivided_1.obj", // Ensure file exists
        white,
        glm::vec3(350.0f, 130.0f, 200.0f), // Pos
        800.0f,                       // Scale
        glm::vec3(0.0f, 1.0f, 0.0f),          // Rot Axis
        170.0f                       // Rot Angle
    );
    // Density 0.01, White color
    world.add(std::make_shared<ConstantMedium>(boundary_bunny, 0.01f, glm::vec3(1.0f)));
    world.add(std::make_shared<ConstantMedium>(boundary_fog, 0.001f, glm::vec3(1.0f), glm::vec3(2.0f, 0.75f, 0.25f)));

    // Camera
    glm::vec3 lookfrom(278.0f, 278.0f, -800.0f);
    glm::vec3 lookat(278.0f, 278.0f, 0);
    float dist_to_focus = 10.0f;
    float aperture = 0.0f;
    cam = Camera(lookfrom, lookat, glm::vec3(0.0f,1.0f,0.0f), 40.0f, aspect, aperture, dist_to_focus);
    
    world.set_background(std::make_shared<SolidColor>(0.0f, 0.0f, 0.0f));
}

// =======================================================================
// Scene 3: Motion Blur & Acceleration
// 验证功能:
// 1. Motion Blur (Moving Spheres)
// 2. BVH (Large number of objects)
// =======================================================================
void scene_motion_blur(Scene& world, Camera& cam, float aspect) {
    world.clear();

    auto ground = std::make_shared<Lambertian>(glm::vec3(0.5f, 0.5f, 0.5f));
    world.add(std::make_shared<Sphere>(glm::vec3(0.0f,-1000.0f,0.0f), 1000.0f, ground));

    // Add many small moving spheres
    for (int a = -11; a < 11; a++) {
        for (int b = -11; b < 11; b++) {
            float choose_mat = random_float();
            glm::vec3 center(a + 0.9f*random_float(), 0.2f, b + 0.9f*random_float());

            if (glm::length(center - glm::vec3(4.0f, 0.2f, 0.0f)) > 0.9f) {
                std::shared_ptr<Material> sphere_material;

                if (choose_mat < 0.8f) {
                    // Diffuse moving sphere
                    glm::vec3 albedo = random_vec3(0.0f,1.0f) * random_vec3(0.0f,1.0f);
                    sphere_material = std::make_shared<Lambertian>(albedo);
                    glm::vec3 center2 = center + glm::vec3(0.0f, random_float(0.0f, 0.5f), 0.0f); // Move Up
                    world.add(std::make_shared<MovingSphere>(center, center2, 0.0f, 1.0f, 0.2f, sphere_material));
                } else {
                    // Glass/Metal static
                    sphere_material = std::make_shared<Dielectric>(glm::vec3(1.0f), 1.5f);
                    world.add(std::make_shared<Sphere>(center, 0.2f, sphere_material));
                }
            }
        }
    }

    auto material1 = std::make_shared<Dielectric>(glm::vec3(1.0f), 1.5f);
    world.add(std::make_shared<Sphere>(glm::vec3(0.0f, 1.0f, 0.0f), 1.0f, material1));

    // Camera with shutter open/close times [0, 1]
    glm::vec3 lookfrom(13.0f, 2.0f, 3.0f);
    glm::vec3 lookat(0.0f, 0.0f, 0.0f);
    float dist_to_focus = 10.0f;
    float aperture = 0.1f;

    cam = Camera(lookfrom, lookat, glm::vec3(0.0f,1.0f,0.0f), 20.0f, aspect, aperture, dist_to_focus, 0.0f, 1.0f);
    
    world.set_background(std::make_shared<SolidColor>(0.7f, 0.8f, 1.0f));
}

// =======================================================================
// Scene 4: Environment Map & Mesh Loading
// 验证功能:
// 1. Mesh Loading (.obj)
// 2. HDR Environment Map (ImageTexture on Background)
// 3. Transformation (Rotation/Scale of Mesh)
// =======================================================================
void scene_mesh_env(Scene& world, Camera& cam, float aspect) {
    world.clear();

    // Background (HDR)
    // 假设有 assets/texture/hdr/puresky_1k.hdr，如果没有则用普通图片或纯色
    world.set_background(std::make_shared<ImageTexture>("assets/envir/qwantani_puresky_1k.hdr"));
    // world.set_background(std::make_shared<SolidColor>(0.1f, 0.1f, 0.15f)); // Fallback

    auto mat_gold = std::make_shared<Metal>(glm::vec3(1.0f, 0.84f, 0.0f), 0.1f);
    auto mat_floor = std::make_shared<Lambertian>(glm::vec3(0.5f));

    // Floor
    world.add(std::make_shared<Sphere>(glm::vec3(0.0f,-1000.0f,0.0f), 1000.0f, mat_floor));

    // Mesh (Bunny)
    auto bunny = std::make_shared<Mesh>(
        "assets/model/bunny_200_subdivided_1.obj", // Ensure file exists
        mat_gold,
        glm::vec3(0.0f, 0.0f, 0.0f), // Pos
        50.0f,                       // Scale
        glm::vec3(0.0f, 1.0f, 0.0f),          // Rot Axis
        180.0f                       // Rot Angle
    );
    world.add(bunny);

    // Light
    world.add(std::make_shared<Sphere>(glm::vec3(0.0f, 50.0f, 0.0f), 10.0f, std::make_shared<DiffuseLight>(glm::vec3(5.0f))));

    // Camera
    glm::vec3 lookfrom(0.0f, 30.0f, 60.0f);
    glm::vec3 lookat(0.0f, 10.0f, 0.0f);
    cam = Camera(lookfrom, lookat, glm::vec3(0.0f,1.0f,0.0f), 40.0f, aspect, 0.0f, 10.0f);
}



// =======================================================================
// Scene 1: Advanced Materials & Textures
// 验证功能: 
// 1. Texture Mapping (Checker, Image, Perlin)
// 2. Normal Mapping (Bump map)
// 3. Material (Glass, Metal, Matte)
// 4. Depth of Field (Aperture > 0)
// =======================================================================
void scene_5(Scene& world, Camera& cam, float aspect) {
    world.clear();

    // Textures
    auto checker = std::make_shared<CheckerTexture>(glm::vec3(0.2f, 0.3f, 0.1f), glm::vec3(0.9f), 10.0f);
    auto perlin  = std::make_shared<Perlin>(4.0f);
    
    // Materials
    auto mat_ground = std::make_shared<Lambertian>(checker);
    auto mat_glass  = std::make_shared<Dielectric>(glm::vec3(1.0f), 1.5f);
    auto mat_metal  = std::make_shared<Metal>(glm::vec3(0.7f, 0.6f, 0.5f), 0.05f);
    auto mat_noise  = std::make_shared<Lambertian>(perlin);
    
    // Ground
    world.add(std::make_shared<Sphere>(glm::vec3(0.0f, -1000.0f, 0.0f), 1000.0f, mat_ground));

    // Objects
    // world.add(std::make_shared<Sphere>(glm::vec3(0, 1, 0), 1.0f, mat_glass));      // Center: Glass
    // world.add(std::make_shared<Sphere>(glm::vec3(-4, 1, 0), 1.0f, mat_noise));     // Left: Perlin Noise
    // world.add(std::make_shared<Sphere>(glm::vec3(4, 1, 0), 1.0f, mat_metal));      // Right: Metal

    // Normal Map Test (Front)
    // 需要确保 assets/texture/red_brick/ 路径下有纹理，否则用纯色替代
    // 这里假设你有图片，如果没有，会自动回退或报错，可根据实际情况注释掉
    auto diff_tex = std::make_shared<ImageTexture>("assets/texture/broken_brick_wall/broken_brick_wall_diff_1k.png");
    auto norm_tex = std::make_shared<ImageTexture>("assets/texture/broken_brick_wall/broken_brick_wall_nor_gl_1k.png");
    auto mat_brick = std::make_shared<Lambertian>(diff_tex);
    mat_brick->set_normal_map(norm_tex);
    world.add(std::make_shared<Sphere>(glm::vec3(4.0f, 1.0f, 0.0f), 1.0f, mat_brick)); 
    auto light_mat = std::make_shared<DiffuseLight>(glm::vec3(15.0f, 15.0f, 15.0f));
    world.add(std::make_shared<Sphere>(glm::vec3(4.0f, 0.0f, 1.3f), 0.5f, light_mat));

    // Camera
    glm::vec3 lookfrom(13.0f, 2.0f, 3.0f);
    glm::vec3 lookat(0.0f, 0.0f, 0.0f);
    float dist_to_focus = 10.0f;
    float aperture = 0.1f; // Enable Depth of Field

    cam = Camera(lookfrom, lookat, glm::vec3(0.0f,1.0f,0.0f), 20.0f, aspect, aperture, dist_to_focus);
    
    // Environment Lighting (Background)
    world.set_background(std::make_shared<SolidColor>(0.7f, 0.8f, 1.0f));
}
