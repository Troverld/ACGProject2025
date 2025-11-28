#include <iostream>
#include <vector>
#include <string>

// External
#include <glm/glm.hpp>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// Project Headers
#include "Core/Utils.hpp"
#include "Core/Ray.hpp"
#include "Scene/Sphere.hpp"
#include "Scene/Camera.hpp"

// 计算光线颜色
// 如果撞到球，返回法线颜色；如果没撞到，返回背景色（天空蓝）
glm::vec3 ray_color(const Ray& r, const Object& world) {
    HitRecord rec;
    // t_min 设为 0.001 避免浮点误差导致的"自遮挡"（Shadow Acne）
    if (world.intersect(r, 0.001f, Infinity, rec)) {
        // 将法线 [-1, 1] 映射到 [0, 1] 作为颜色显示
        return 0.5f * (rec.normal + glm::vec3(1.0f));
    }

    // 背景：线性渐变
    // y 方向归一化到 0-1
    glm::vec3 unit_direction = glm::normalize(r.direction());
    float t = 0.5f * (unit_direction.y + 1.0f);
    // 混合 白色(1,1,1) 和 蓝色(0.5, 0.7, 1.0)
    return (1.0f - t) * glm::vec3(1.0f, 1.0f, 1.0f) + t * glm::vec3(0.5f, 0.7f, 1.0f);
}

int main() {
    // 1. 图像参数
    const float aspect_ratio = 16.0f / 9.0f;
    const int width = 800;
    const int height = static_cast<int>(width / aspect_ratio);
    const int channels = 3;

    std::cout << "Rendering " << width << "x" << height << "..." << std::endl;

    // 2. 场景构建
    // 创建一个球体：位置(0,0,-1)，半径0.5
    // 注意相机默认看向 -Z 方向
    Sphere sphere(glm::vec3(0.0f, 0.0f, -1.0f), 0.5f);

    // 3. 相机构建
    glm::vec3 lookfrom(0.0f, 0.0f, 1.0f); // 相机在 Z=1
    glm::vec3 lookat(0.0f, 0.0f, -1.0f);  // 看向球体
    glm::vec3 vup(0.0f, 1.0f, 0.0f);
    float fov = 90.0f;
    Camera cam(lookfrom, lookat, vup, fov, aspect_ratio);

    // 4. 渲染循环
    std::vector<unsigned char> image(width * height * channels);

    for (int j = 0; j < height; ++j) {
        // 打印进度条
        if (j % 50 == 0) std::cout << "Scanning line " << j << std::endl;

        for (int i = 0; i < width; ++i) {
            // UV 坐标 (0,0 在左下角)
            float u = float(i) / (width - 1);
            float v = float(height - 1 - j) / (height - 1); // 注意：stb 是从上往下写，所以 v 要反转

            // 发射光线
            Ray r = cam.get_ray(u, v);
            
            // 计算颜色
            glm::vec3 pixel_color = ray_color(r, sphere);

            // 写入 Buffer (Gamma 矫正暂略，直接写线性值)
            int index = (j * width + i) * channels;
            image[index + 0] = static_cast<unsigned char>(255.999f * pixel_color.r);
            image[index + 1] = static_cast<unsigned char>(255.999f * pixel_color.g);
            image[index + 2] = static_cast<unsigned char>(255.999f * pixel_color.b);
        }
    }

    // 5. 保存
    stbi_write_png("phase2_output.png", width, height, channels, image.data(), width * channels);
    std::cout << "Done! Saved to phase2_output.png" << std::endl;

    return 0;
}