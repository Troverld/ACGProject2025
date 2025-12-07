#pragma once

#include <glm/glm.hpp>
#include <memory>
#include "../core/utils.hpp"
#include "../core/ray.hpp"
#include "../accel/AABB.hpp"
#include "../core/record.hpp"

/**
 * @brief Abstract base class for all renderable objects in the scene.
 */
class Object {
public:
    virtual ~Object() = default;

    /**
     * @brief Tests for intersection between a ray and the object.
     * 
     * @param r The ray to test.
     * @param t_min The minimum valid t value (to prevent shadow acne).
     * @param t_max The maximum valid t value.
     * @param rec Output parameter to store intersection details.
     * @return true If an intersection occurred within [t_min, t_max].
     * @return false Otherwise.
     */
    virtual bool intersect(const Ray& r, float t_min, float t_max, HitRecord& rec) const = 0;

    /**
     * @brief Compute the bounding box of the object.
     * Essential for BVH construction.
     * 
     * @param time0 Start time for motion blur interval.
     * @param time1 End time for motion blur interval.
     * @param output_box The calculated AABB.
     * @return true If the object has a bounding box (infinite planes might not).
     */
    virtual bool bounding_box(float time0, float time1, AABB& output_box) const = 0;

     /**
     * @brief Calculates the probability density function value of a direction.
     * Used when we want to know: "What was the probability of picking direction v towards this object?"
     */
    virtual float pdf_value(const glm::vec3& origin, const glm::vec3& v) const {
        return 0.0f;
    }

    /**
     * @brief Generates a random vector starting at 'origin' towards a point on this object.
     * IMPORTANT: The returned vector's length must represent the distance to the surface point.
     * 
     * @param origin The point from which we are viewing the object.
     * @return glm::vec3 Vector P_surface - origin.
     */
    virtual glm::vec3 random_pointing_vector(const glm::vec3& origin) const {
        return glm::vec3(1, 0, 0);
    }

    /**
     * @brief Randomly sample a point and normal on the surface of object.
     * @param pos [out] Sampled point (world space)
     * @param normal [out] Geometric normal (normalized).
     * @param pdf_area [out] pdf of area.
     */
    virtual void sample_surface(glm::vec3& pos, glm::vec3& normal, float& pdf_area) const {
        pdf_area = 0.0f; 
    }
    
    /**
     * @brief Helper to access material pointer.
     */
    virtual Material* get_material() const = 0;
};