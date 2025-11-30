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

    /**
     * @brief Computes PDF of sampling a direction towards this sphere.
     * This uses the solid angle of the cone subtended by the sphere.
     */
    virtual float pdf_value(const glm::vec3& o, const glm::vec3& v) const override {
        HitRecord rec;
        // If ray doesn't hit this sphere, PDF is 0
        if (!this->intersect(Ray(o, v), 0.001f, Infinity, rec))
            return 0.0f;

        glm::vec3 direction = center - o;
        float cos_theta_max = sqrt(1 - radius * radius / glm::dot(direction, direction));
        float solid_angle = 2 * PI * (1 - cos_theta_max);

        return 1.0f / solid_angle;
    }

    /**
     * @brief Generates a random direction towards the sphere.
     * Uniformly samples the solid angle subtended by the sphere.
    */
    virtual glm::vec3 random_pointing_vector(const glm::vec3& o) const override {
        glm::vec3 direction = center - o;
        float distance_squared = glm::dot(direction, direction);
        
        // Construct a local coordinate system (ONB)
        glm::vec3 w = glm::normalize(direction);
        glm::vec3 a = (fabs(w.x) > 0.9f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
        glm::vec3 v = glm::normalize(glm::cross(w, a));
        glm::vec3 u = glm::cross(w, v);

        // Sample uniform cone
        float r1 = random_float();
        float r2 = random_float();
        float z = 1 + r2 * (sqrt(1 - radius * radius / distance_squared) - 1);
        float phi = 2 * PI * r1;
        float x = cos(phi) * sqrt(1 - z * z);
        float y = sin(phi) * sqrt(1 - z * z);

        // Transform to world space
        return x * u + y * v + z * w; // Return the vector, not normalized is fine for now as it indicates position relative
    }
    
    virtual Material* get_material() const override { return mat_ptr.get(); }

private:
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

public:
    glm::vec3 center;
    float radius;
    std::shared_ptr<Material> mat_ptr;
};