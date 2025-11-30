#pragma once

#include "../core/utils.hpp"
#include "../core/ray.hpp"
#include <glm/glm.hpp>

/**
 * @brief Axis-Aligned Bounding Box.
 * Essential for BVH construction and fast intersection culling.
 */
class AABB {
public:
    AABB() {}
    AABB(const glm::vec3& a, const glm::vec3& b) {
        bounds_min = a;
        bounds_max = b;
    }

    glm::vec3 min_point() const { return bounds_min; }
    glm::vec3 max_point() const { return bounds_max; }

    /**
     * @brief Check if a ray hits this bounding box.
     * Vectorized implementation: eliminates loops and explicit branching.
     */
    bool hit(const Ray& r, float t_min, float t_max) const {
        // Calculate intersection distances for all 3 planes at once
        // No division here (precomputed in Ray)
        const glm::vec3 t0 = (bounds_min - r.origin()) * r.inv_direction();
        const glm::vec3 t1 = (bounds_max - r.origin()) * r.inv_direction();

        // Implicitly handle the "swap" for negative directions.
        // glm::min/max perform component-wise comparison.
        const glm::vec3 t_smaller = glm::min(t0, t1);
        const glm::vec3 t_bigger  = glm::max(t0, t1);

        // Find the Latest Entry time (max of all entry times)
        // using C++17 initializer list for std::max
        const float t_enter = std::max({t_min, t_smaller.x, t_smaller.y, t_smaller.z});

        // Find the Earliest Exit time (min of all exit times)
        const float t_exit  = std::min({t_max, t_bigger.x,  t_bigger.y,  t_bigger.z});

        // If entry is before exit, we have a hit
        return t_enter < t_exit;
    }

public:
    glm::vec3 bounds_min;
    glm::vec3 bounds_max;
};

/**
 * @brief Computes the bounding box surrounding two boxes.
 */
inline AABB surrounding_box(const AABB& box0, const AABB& box1) {
    glm::vec3 small(fmin(box0.min_point().x, box1.min_point().x),
                    fmin(box0.min_point().y, box1.min_point().y),
                    fmin(box0.min_point().z, box1.min_point().z));

    glm::vec3 big(fmax(box0.max_point().x, box1.max_point().x),
                  fmax(box0.max_point().y, box1.max_point().y),
                  fmax(box0.max_point().z, box1.max_point().z));

    return AABB(small, big);
}