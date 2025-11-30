#pragma once

#include "../core/ray.hpp"
#include <glm/glm.hpp>
#include "../accel/AABB.hpp"
#include <memory>

// Forward declaration to avoid circular dependency
class Material;

/**
 * @brief Structure to store information about a ray-object intersection.
 */
struct HitRecord {
    glm::vec3 p;                  ///< Intersection point in world space.
    glm::vec3 normal;             ///< Surface normal at intersection.
    Material* mat_ptr;            ///< Pointer to the material of the hit object. Use a raw pointer instead of shared_ptr to reduce overhead.
    float t;                      ///< Ray parameter t where intersection occurred.
    bool front_face;              ///< True if ray hit the front face, false if back face.
    float u;                      ///< Texture coordinate U [0,1].
    float v;                      ///< Texture coordinate V [0,1].

    /**
     * @brief Sets the hit record normal and front_face flag based on ray direction.
     * Ensures the normal always points against the ray (outward for external hits).
     * 
     * @param r The incoming ray.
     * @param outward_normal The geometric normal (usually pointing out of the object).
     */
    void set_face_normal(const Ray& r, const glm::vec3& outward_normal) {
        front_face = glm::dot(r.direction(), outward_normal) < 0;
        normal = front_face ? outward_normal : -outward_normal;
    }
};

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
};