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

    /**
     * @brief Calculates UV coordinates for a sphere.
     * Maps (x,y,z) to (u,v) in [0,1].
     */
    static void get_sphere_uv(const glm::vec3& p, float& u, float& v) {
        // p: a given point on the sphere of radius one, centered at the origin.
        // u: returned value [0,1] of angle around the Y axis from X=-1.
        // v: returned value [0,1] of angle from Y=-1 to Y=+1.
        //     <1 0 0> yields <0.50 0.50>       <-1  0  0> yields <0.00 0.50>
        //     <0 1 0> yields <0.50 1.00>       < 0 -1  0> yields <0.50 0.00>
        //     <0 0 1> yields <0.25 0.50>       < 0  0 -1> yields <0.75 0.50>

        float theta = acos(-p.y);
        float phi = atan2(-p.z, p.x) + PI;

        u = phi / (2 * PI);
        v = theta / PI;
    }

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
        rec.mat_ptr = mat_ptr.get(); // Assign the material to the hit record. Use .get() to extract the raw pointer, so that copy cost of shared_ptr is avoided.

        return true;
    }
    /**
     * @brief Find the AABB of a sphere.
     */
    virtual bool bounding_box(float time0, float time1, AABB& output_box) const override {
        output_box = AABB(
            center - glm::vec3(radius, radius, radius),
            center + glm::vec3(radius, radius, radius)
        );
        return true;
    }

public:
    glm::vec3 center;
    float radius;
    std::shared_ptr<Material> mat_ptr;
};