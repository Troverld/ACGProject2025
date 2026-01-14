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
    auto mat_brick = std::make_shared<Lambertian>(diff_tex, norm_tex);
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

    // world.add_light(std::make_shared<PointLight>(glm::vec3(120.0f,239.0f,127.0f), glm::vec3(4e3f, 8e3f, 1e3f)));

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
    auto mat_brick = std::make_shared<Lambertian>(diff_tex, norm_tex);
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


// =======================================================================
// Scene 6: Chromatic Dispersion Verification
// Verification:
// 1. Stochastic Spectral Sampling (Rainbow edges)
// 2. Cauchy's Equation (High dispersion glass)
// 3. Spectral Caustics (if using PhotonIntegrator)
// =======================================================================
void scene_dispersion(Scene& world, Camera& cam, float aspect) {
    world.clear();

    // 1. Dark background to make spectral colors pop
    world.set_background(std::make_shared<SolidColor>(0.02f, 0.02f, 0.05f));

    // 2. Dispersive Sphere (The "Prism")
    // A = 1.50 (Base Index, typical glass)
    // B = 0.02 (Dispersion Strength). 
    // Note: Real BK7 glass has B ~ 0.004. We use 0.02 to exaggerate the effect for demonstration.
    auto mat_heavy_dispersion = std::make_shared<DispersiveGlass>(glm::vec3(1.0f), 1.50f, 0.02f);
    world.add(std::make_shared<Sphere>(glm::vec3(0.0f, 1.0f, 0.0f), 1.0f, mat_heavy_dispersion));

    // 3. Regular Glass Sphere (Left) for comparison
    auto mat_regular_glass = std::make_shared<Dielectric>(glm::vec3(1.0f), 1.5f);
    world.add(std::make_shared<Sphere>(glm::vec3(-2.5f, 1.0f, 0.0f), 0.8f, mat_regular_glass));

    // 4. Floor with Grid Texture (helps visualize refractive distortion)
    auto checker = std::make_shared<CheckerTexture>(glm::vec3(0.1f), glm::vec3(0.6f), 5.0f);
    world.add(std::make_shared<Sphere>(glm::vec3(0.0f,-1000.0f,0.0f), 1000.0f, std::make_shared<Lambertian>(checker)));

    // 5. Strong Back/Side Light
    // A strong, small light source creates sharp incident angles, ideal for splitting the spectrum.
    auto strong_light = std::make_shared<DiffuseLight>(glm::vec3(30.0f));
    world.add(std::make_shared<Sphere>(glm::vec3(2.0f, 4.0f, -3.0f), 0.5f, strong_light));

    // 6. Camera Setup
    // Positioned to look through the sphere edges where dispersion is strongest.
    glm::vec3 lookfrom(0.0f, 2.5f, 4.0f);
    glm::vec3 lookat(0.0f, 1.0f, 0.0f);
    float dist_to_focus = glm::length(lookfrom - lookat);
    
    // Slight aperture to smooth out noise, but keep it small to keep spectral edges sharp.
    cam = Camera(lookfrom, lookat, glm::vec3(0,1,0), 35.0f, aspect, 0.02f, dist_to_focus);
}


// =======================================================================
// Scene 7: Prism Spectroscopy (The "Pink Floyd" Setup)
// Features:
// 1. Manually constructed Triangular Prism
// 2. High dispersion (Flint glass simulation)
// 3. Projection of spectrum onto the floor
// =======================================================================
void scene_prism_spectrum(Scene& world, Camera& cam, float aspect) {
    world.clear();

    // 1. Dark Room (No ambient light)
    world.set_background(std::make_shared<ImageTexture>("assets/envir/NightSkyHDRI008_4K_HDR.hdr"));

    // 2. The Projection Screen (The Floor/Table)
    // We make it matte white to catch the rainbow clearly.
    // auto mat_screen = std::make_shared<Lambertian>(glm::vec3(0.8f));

    auto desk_diff_tex = std::make_shared<ImageTexture>("assets/texture/painted_wood/PaintedWood007C_1K-PNG_Color.png");
    auto desk_norm_tex = std::make_shared<ImageTexture>("assets/texture/painted_wood/PaintedWood007C_1K-PNG_NormalGL.png");
    auto mat_desk = std::make_shared<Lambertian>(desk_diff_tex, desk_norm_tex);
    // A long floor stretching along X to catch the refracted beam
    world.add(std::make_shared<Triangle>(glm::vec3(-10,-0.5f,-5), glm::vec3(10,-0.5f,-5), glm::vec3(10,-0.5f,5), mat_desk)); 
    world.add(std::make_shared<Triangle>(glm::vec3(-10,-0.5f,-5), glm::vec3(10,-0.5f,5), glm::vec3(-10,-0.5f,5), mat_desk));
    
    auto ground_diff_tex = std::make_shared<ImageTexture>("assets/texture/rocky_terrain/rocky_terrain_02_diff_1k.png");
    auto ground_norm_tex = std::make_shared<ImageTexture>("assets/texture/rocky_terrain/rocky_terrain_02_nor_gl_1k.png");
    auto mat_ground = std::make_shared<Lambertian>(ground_diff_tex, ground_norm_tex);
    world.add(std::make_shared<Disk>(glm::vec3(0.0f,-5.0f,0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 75.0f, mat_ground)); 

    // 3. The Prism Material
    // High dispersion parameters to exaggerate the rainbow spread over a short distance.
    // Cauchy B = 0.05 is extremely high (heavier than flint glass), ideal for distinct bands.
    auto mat_prism = std::make_shared<DispersiveGlass>(glm::vec3(1.0f), 1.50f, 0.05f);

    // 4. Constructing the Prism Geometry (Vertices)
    // An equilateral prism lying on the floor, extending along Z axis.
    float height = 2.0f;
    float width  = 1.5f;
    float depth  = 1.0f; 

    // Front triangle (Z = depth)
    glm::vec3 p0(-width/2, 0.0f,   depth);
    glm::vec3 p1( width/2, 0.0f,   depth);
    glm::vec3 p2( 0.0f,    height, depth);

    // Back triangle (Z = -depth)
    glm::vec3 p3(-width/2, 0.0f,   -depth);
    glm::vec3 p4( width/2, 0.0f,   -depth);
    glm::vec3 p5( 0.0f,    height, -depth);

    // Add Triangles (Counter-Clockwise winding usually)
    // Front Face
    world.add(std::make_shared<Triangle>(p0, p1, p2, mat_prism));
    // Back Face
    world.add(std::make_shared<Triangle>(p3, p5, p4, mat_prism));
    // Bottom Face (Rect: p0-p1-p4-p3)
    world.add(std::make_shared<Triangle>(p0, p4, p3, mat_prism));
    world.add(std::make_shared<Triangle>(p0, p1, p4, mat_prism));
    // Left Face (Rect: p0-p2-p5-p3)
    world.add(std::make_shared<Triangle>(p0, p2, p5, mat_prism));
    world.add(std::make_shared<Triangle>(p0, p5, p3, mat_prism));
    // Right Face (Rect: p1-p4-p5-p2)
    world.add(std::make_shared<Triangle>(p1, p4, p5, mat_prism));
    world.add(std::make_shared<Triangle>(p1, p5, p2, mat_prism));

    // 5. The Light Source (The "Beam")
    // We position a bright, small sphere high up and to the left.
    // It acts like a spotlight directed at the prism.
    auto light_intensity = glm::vec3(1000.0f); // Very bright to compensate for distance
    auto mat_light = std::make_shared<DiffuseLight>(light_intensity);
    
    // Position: Left (-X), Up (+Y).
    // The angle is crucial. We want roughly 45-60 degrees incidence to maximize dispersion width.
    // world.add(std::make_shared<Sphere>(glm::vec3(-8.0f, 2.0f, -0.1f), 0.2f, mat_light));
    glm::vec3 lightCenter(-10.0f, 1.8f, 0.1f);
    glm::vec3 prismCenter(0.0f, 1.0f, 0.0f);
    
    glm::vec3 forward = glm::normalize(prismCenter - lightCenter);
    
    glm::vec3 worldUp = (std::abs(forward.y) > 0.9f) ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
    
    glm::vec3 right = glm::normalize(glm::cross(forward, worldUp));
    glm::vec3 up    = glm::normalize(glm::cross(right, forward));
    
    float lightSize = 0.25f;
    glm::vec3 l_p0 = lightCenter + (up * lightSize);
    glm::vec3 l_p1 = lightCenter + (up * -0.5f * lightSize) - (right * 0.866f * lightSize);
    glm::vec3 l_p2 = lightCenter + (up * -0.5f * lightSize) + (right * 0.866f * lightSize);

    auto mat_light_tri = std::make_shared<DiffuseLight>(glm::vec3(5500.0f)); 

    world.add(std::make_shared<Triangle>(l_p0, l_p2, l_p1, mat_light_tri));

    auto newton_mesh = std::make_shared<Mesh>(
        "assets/model/newton/newton.obj",
        nullptr,
        glm::vec3(3.0f, 1.0f, -2.5f),
        3.0f,
        glm::vec3(0, 1, 0),
        0.0f
    );
    auto apples_mesh = std::make_shared<MovingMesh>(
        "assets/model/apples/apples.obj",
        glm::vec3(2.7f, 3.1f, -2.3f),
        glm::vec3(2.7f, 2.8f, -2.3f),
        0.0f, 1.0f,
        nullptr,
        1.0f,
        glm::vec3(0, 1.0f, 0.0f),
        0.0f
    );
    auto tree_mesh = std::make_shared<Mesh>(
        "assets/model/tree/tree.obj",
        nullptr,
        glm::vec3(5.5f, -5.0f, -6.5f),
        5.0f,
        glm::vec3(0, 1, 0),
        0.0f
    );
    auto telescope_mesh = std::make_shared<Mesh>(
        "assets/model/telescope/telescope.obj",
        nullptr,
        glm::vec3(-1.0f, -3.0f, -17.0f),
        0.05f,
        glm::vec3(0, 1, 0),
        80.0f
    );
    auto scroll_mesh = std::make_shared<Mesh>(
        "assets/model/scroll/scroll.obj",
        nullptr,
        glm::vec3(3.5f, -0.49f, 2.1f),
        0.35f,
        glm::vec3(0, 1, 0),
        100.0f
    );
    auto bible_mesh = std::make_shared<Mesh>(
        "assets/model/bible/bible.obj",
        nullptr,
        glm::vec3(0.5f, -0.49f, 3.1f),
        1.35f,
        glm::vec3(0, 1, 0),
        120.0f
    );
    auto coinstack_mesh = std::make_shared<Mesh>(
        "assets/model/coinstack/coinstack.obj",
        nullptr,
        glm::vec3(2.1f, -0.49f, -2.3f),
        3.35f,
        glm::vec3(0, 1, 0),
        135.0f
    );
    auto inkwell_mesh = std::make_shared<Mesh>(
        "assets/model/inkwell/inkwell.obj",
        nullptr,
        glm::vec3(5.0f, -0.49f, 3.2f),
        0.7f,
        glm::vec3(0, 1, 0),
        10.0f
    );
    auto compass_mesh = std::make_shared<Mesh>(
        "assets/model/compass/compass.obj",
        nullptr,
        glm::vec3(4.5f, -0.4f, -2.5f),
        0.25f,
        glm::vec3(0, 1, 0),
        -45.0f
    );
    world.add(newton_mesh);
    world.add(apples_mesh);
    world.add(tree_mesh);
    world.add(scroll_mesh);
    world.add(telescope_mesh);
    world.add(bible_mesh);
    world.add(coinstack_mesh);
    world.add(inkwell_mesh);
    world.add(compass_mesh);

    // 6. Camera
    // Look from the side to see both the prism and the projected spectrum on the floor.
    glm::vec3 lookfrom(0.0f, 5.0f, 12.0f);
    glm::vec3 lookat(3.0f, 0.3f, -4.0f); // Look at the area where rainbow lands
    
    cam = Camera(lookfrom, lookat, glm::vec3(0,1,0), 30.0f, aspect, 0.0f, 10.0f, 0.0f, 1.0f);
}


// =======================================================================
// Scene 8: Newton Bust Import Test
// 验证功能:
// 1. OBJ Import Integrity (Newton bust)
// 2. BVH Build Efficiency (Complex geometry check)
// 3. Scale & Orientation Verification
// =======================================================================
void scene_newton_test(Scene& world, Camera& cam, float aspect) {
    world.clear();

    // 1. Setup Materials
    // 使用纯白 Lambertian 材质模拟石膏像，这最能检验光照计算和法线插值是否正确
    auto mat_plaster = std::make_shared<Lambertian>(glm::vec3(0.85f, 0.85f, 0.85f));
    auto mat_floor   = std::make_shared<Lambertian>(glm::vec3(0.2f, 0.2f, 0.2f)); // 深灰地板
    auto mat_light   = std::make_shared<DiffuseLight>(glm::vec3(15.0f)); // 强光
    auto mat_gold = std::make_shared<Metal>(glm::vec3(1.0f, 0.84f, 0.0f), 0.1f);

    // 2. Environment
    // 使用暗色背景，模拟摄影棚环境，避免环境光干扰对模型的观察
    world.set_background(std::make_shared<SolidColor>(0.05f, 0.05f, 0.05f)); 

    // Floor (防止模型悬空，并接收投影)
    // 地板放在 Y = 0 下方一点点，假设模型底座在 Y=0
    // world.add(std::make_shared<Sphere>(glm::vec3(0.0f,-1000.0f,0.0f), 1000.0f, mat_floor));
    world.add(std::make_shared<Triangle>(glm::vec3(-20,0.0f,-10), glm::vec3(20,0.0f,-10), glm::vec3(20,0.0f,10), mat_floor)); 
    world.add(std::make_shared<Triangle>(glm::vec3(-20,0.0f,-10), glm::vec3(20,0.0f,10), glm::vec3(-20,0.0f,10), mat_floor));

    // 3. The Newton Model
    // 关键调试点：Scale (缩放)
    // Three.js 导出的 OBJ 通常单位比较统一，但取决于原始扫描数据的单位（米/厘米/毫米）。
    // 策略：先假设单位是 "米" (Scale = 1.0)。
    // 如果渲染出来是全黑：
    //   a. 模型太小 -> 改为 10.0f 或 100.0f
    //   b. 模型太大(相机在肚子里) -> 改为 0.1f 或 0.01f
    float model_scale = 1.0f; 
    
    // 关键调试点：Rotation (旋转)
    // 很多扫描模型默认是 Y-up，但也有些是 Z-up 导致躺在地上。
    // 如果模型躺着，尝试绕 X 轴旋转 -90 或 90 度。
    auto newton_mesh = std::make_shared<Mesh>(
        "assets/model/newton/newton.obj",  // 确保路径正确
        // mat_plaster,                // 暂时忽略纹理，强制使用白模，专注于几何体测试
        nullptr,
        glm::vec3(0.0f, 1.0f, 0.0f),// Position (底座中心置于原点)
        model_scale,                // Scale
        glm::vec3(0, 1, 0),         // Rotation Axis (绕 Y 轴)
        0.0f                        // Rotation Angle (根据朝向调整，例如 180.0f)
    );
    world.add(newton_mesh);
    world.add(std::make_shared<Sphere>(glm::vec3(-3.0f, 1.0f, 0.0f), 1.0f, mat_gold));

    // 4. Lighting
    // 经典的“伦勃朗光”位：右上方 45 度打光，能很好地表现面部体积感
    world.add(std::make_shared<Sphere>(glm::vec3(5.0f, 6.0f, 5.0f), 1.5f, mat_light));

    // 5. Camera
    // 根据 Three.js 的截图，这是一个半身像。
    // 假设模型高度约 1.0 - 2.0 单位。
    // 摄像机位置：稍微俯视，保持一定距离。
    glm::vec3 lookfrom(0.0f, 3.5f, 6.0f); 
    glm::vec3 lookat(0.0f, 1.2f, 0.0f);   // 看向大概头部的位置 (假设头部在 Y=1.2 左右)
    
    // 禁用景深 (aperture = 0)，确保在调试阶段全图清晰
    cam = Camera(lookfrom, lookat, glm::vec3(0,1,0), 30.0f, aspect, 0.0f, 10.0f);
}