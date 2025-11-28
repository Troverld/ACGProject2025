#pragma once

#include "object.hpp"
#include <glm/glm.hpp>
#include <memory>

/**
 * @brief A Sphere object defined by a center and a radius.
 */
class Sphere : public Object {
public:
    Sphere() {}
    
    /**
     * @brief Construct a new Sphere.
     * 
     * @param cen Center position.
     * @param r Radius.
     * @param m Pointer to the material.
     */
    Sphere(glm::vec3 cen, float r, std::shared_ptr<Material> m) 
        : center(cen), radius(r), mat_ptr(m) {};

    virtual bool intersect(const Ray& r, float t_min, float t_max, HitRecord& rec) const override {
        glm::vec3 oc = r.origin() - center;
        float a = glm::dot(r.direction(), r.direction());
        float half_b = glm::dot(oc, r.direction());
        float c = glm::dot(oc, oc) - radius * radius;
        
        float discriminant = half_b * half_b - a * c;
        if (discriminant < 0) return false;
        
        float sqrtd = sqrt(discriminant);

        // Find the nearest root that lies in the acceptable range.
        float root = (-half_b - sqrtd) / a;
        if (root < t_min || root > t_max) {
            root = (-half_b + sqrtd) / a;
            if (root < t_min || root > t_max)
                return false;
        }

        rec.t = root;
        rec.p = r.at(rec.t);
        glm::vec3 outward_normal = (rec.p - center) / radius;
        rec.set_face_normal(r, outward_normal);
        rec.mat_ptr = mat_ptr; // Assign the material to the hit record

        return true;
    }

public:
    glm::vec3 center;
    float radius;
    std::shared_ptr<Material> mat_ptr;
};