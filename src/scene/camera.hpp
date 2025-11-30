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
     * @param lookfrom Position of the camera.
     * @param lookat Point the camera is looking at.
     * @param vup Up vector.
     * @param vfov Vertical field of view (degrees).
     * @param aspect_ratio Screen width / height.
     * @param aperture Aperture diameter (for depth of field). 0 = Pinhole.
     * @param focus_dist Distance to focus plane.
     * @param t0 Shutter open time.
     * @param t1 Shutter close time.
     */
    Camera(
        glm::vec3 lookfrom, 
        glm::vec3 lookat, 
        glm::vec3 vup, 
        float vfov, 
        float aspect_ratio,
        float aperture = 0.0f,
        float focus_dist = 10.0f,
        float t0 = 0.0,
        float t1 = 0.0
    ) : time0(t0), time1(t1) {
        float theta = glm::radians(vfov);
        float h = tan(theta / 2);
        float viewport_height = 2.0f * h;
        float viewport_width = aspect_ratio * viewport_height;

        w = glm::normalize(lookfrom - lookat);
        u = glm::normalize(glm::cross(vup, w));
        v = glm::cross(w, u);

        origin = lookfrom;
        horizontal = focus_dist * viewport_width * u;
        vertical = focus_dist * viewport_height * v;
        lower_left_corner = origin - horizontal / 2.0f - vertical / 2.0f - focus_dist * w;

        lens_radius = aperture / 2.0f;
    }

    /**
     * @brief Full featured camera with Defocus Blur and Motion Blur support.
     * 
     * @param s Horizontal screen coordinate (0-1).
     * @param t Vertical screen coordinate (0-1).
     * @return Ray The ray starting from camera origin passing through (s,t).
     */
    Ray get_ray(float s, float t) const {
        glm::vec3 rd = lens_radius * random_in_unit_disk();
        glm::vec3 offset = u * rd.x + v * rd.y;

        return Ray(
            origin + offset,
            lower_left_corner + s * horizontal + t * vertical - origin - offset,
            random_float(time0, time1)
        );
    }

private:
    glm::vec3 origin;
    glm::vec3 lower_left_corner;
    glm::vec3 horizontal;
    glm::vec3 vertical;
    glm::vec3 u, v, w;
    float lens_radius;
    float time0, time1; // Shutter open/close times
};