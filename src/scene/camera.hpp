#pragma once

#include "../core/utils.hpp"
#include "../core/ray.hpp"
#include <glm/glm.hpp>

/**
 * @brief A simple perspective camera.
 */
class Camera {
public:
    /**
     * @brief Construct a new Camera.
     * 
     * @param lookfrom Camera position.
     * @param lookat Point the camera is looking at.
     * @param vup Vector pointing 'up' for the camera (usually 0,1,0).
     * @param vfov Vertical field of view in degrees.
     * @param aspect_ratio Width / Height.
     */
    Camera(glm::vec3 lookfrom, glm::vec3 lookat, glm::vec3 vup, float vfov, float aspect_ratio) {
        float theta = glm::radians(vfov);
        float h = tan(theta / 2);
        float viewport_height = 2.0f * h;
        float viewport_width = aspect_ratio * viewport_height;

        w = glm::normalize(lookfrom - lookat);
        u = glm::normalize(glm::cross(vup, w));
        v = glm::cross(w, u);

        origin = lookfrom;
        horizontal = viewport_width * u;
        vertical = viewport_height * v;
        lower_left_corner = origin - horizontal / 2.0f - vertical / 2.0f - w;
    }

    /**
     * @brief Generate a ray for a specific pixel coordinate.
     * 
     * @param s Horizontal screen coordinate (0-1).
     * @param t Vertical screen coordinate (0-1).
     * @return Ray The ray starting from camera origin passing through (s,t).
     */
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