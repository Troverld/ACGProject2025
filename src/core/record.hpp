#pragma once

#include <glm/glm.hpp>
#include "ray.hpp"
class Object; 
class Material;

/**
 * @brief Structure to store information about a ray-object intersection.
 */
struct HitRecord {
    glm::vec3 p;                  ///< Intersection point in world space.
    glm::vec3 normal;             ///< Surface normal at intersection.
    glm::vec3 tangent;            ///< Surface tangent vector (aligned with U coordinate). 
    Material* mat_ptr;            ///< Pointer to the material of the hit object. Use a raw pointer instead of shared_ptr to reduce overhead.
    float t;                      ///< Ray parameter t where intersection occurred.
    bool front_face;              ///< True if ray hit the front face, false if back face.
    float u;                      ///< Texture coordinate U [0,1].
    float v;                      ///< Texture coordinate V [0,1].
    const Object* object = nullptr; ///< Pointer to the geometric object hit. (Added for MIS)

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
 * @brief Class to record detailed information of a scatter.
 */
struct ScatterRecord {
    Ray specular_ray;       // The reflected ray if the surface is mirror.
    bool is_specular;       // Whether the surface is mirror.
    glm::vec3 attenuation;  // Albedo.
    float pdf;              // If the surface is not mirror, then the PDF of the sampled direction is recorded.
    glm::vec3 shading_normal;  // Perturbed normal from normal map (if any).
    ScatterRecord(glm::vec3 normal) : shading_normal(normal) {} // A normal initialization is a must. We should always have a valid normal.
};