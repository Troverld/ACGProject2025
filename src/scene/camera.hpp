#pragma once

#include "../core/utils.hpp"
#include "../core/ray.hpp"
#include <glm/glm.hpp>

class Camera {
public:
    // vfov: 垂直视场角 (degrees)
    // aspect_ratio: 宽/高
    Camera(glm::vec3 lookfrom, glm::vec3 lookat, glm::vec3 vup, float vfov, float aspect_ratio) {
        // 修改点：使用 GLM 函数
        float theta = glm::radians(vfov);
        
        float h = tan(theta / 2);
        float viewport_height = 2.0f * h;
        float viewport_width = aspect_ratio * viewport_height;

        // 构建相机坐标系 (uvw)
        w = glm::normalize(lookfrom - lookat);
        u = glm::normalize(glm::cross(vup, w));
        v = glm::cross(w, u);

        origin = lookfrom;
        horizontal = viewport_width * u;
        vertical = viewport_height * v;
        lower_left_corner = origin - horizontal / 2.0f - vertical / 2.0f - w;
    }

    Ray get_ray(float s, float t) const {
        return Ray(origin, lower_left_corner + s * horizontal + t * vertical - origin);
    }

private:
    glm::vec3 origin;
    glm::vec3 lower_left_corner;
    glm::vec3 horizontal;
    glm::vec3 vertical;
    glm::vec3 u, v, w;
};