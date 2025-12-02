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
     * @brief Double precision ray-sphere intersection to reduce artifacts.
     */
    virtual bool intersect(const Ray& r, float t_min, float t_max, HitRecord& rec) const override {
        glm::vec3 oc = r.origin() - center;
        
        // Use double precision to prevent striping artifacts on large spheres (like the ground)
        double a = glm::dot(r.direction(), r.direction());
        double half_b = glm::dot(oc, r.direction());
        double c = glm::dot(oc, oc) - double(radius) * double(radius); // Cast to double before multiplication
        
        double discriminant = half_b * half_b - a * c;
        if (discriminant < 0) return false;
        
        double sqrtd = sqrt(discriminant);

        // Find the nearest root that lies in the acceptable range.
        double root = (-half_b - sqrtd) / a;
        if (root < t_min || root > t_max) {
            root = (-half_b + sqrtd) / a;
            if (root < t_min || root > t_max)
                return false;
        }

        // Cast back to float for the record
        rec.t = static_cast<float>(root);
        rec.p = r.at(rec.t);
        
        glm::vec3 outward_normal = (rec.p - center) / radius;
        rec.set_face_normal(r, outward_normal);
        get_sphere_uv(outward_normal, rec.u, rec.v); 
        rec.mat_ptr = mat_ptr.get();
        rec.object = this;

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
     * @brief Generates a random vector from origin TO the sphere surface.
     * Fixed: Now returns a vector with length equal to the distance, not normalized.
     */
    virtual glm::vec3 random_pointing_vector(const glm::vec3& o) const override {
        glm::vec3 direction = center - o;
        float dist_squared = glm::dot(direction, direction);
        
        // Safety check: if inside the sphere, return center (or handle gracefully)
        if (dist_squared <= radius * radius) {
             return center - o; 
        }

        // 1. Construct a local coordinate system (ONB)
        // defined by the vector pointing to the sphere center
        glm::vec3 w = glm::normalize(direction);
        glm::vec3 a = (fabs(w.x) > 0.9f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
        glm::vec3 v = glm::normalize(glm::cross(w, a));
        glm::vec3 u = glm::cross(w, v);

        // 2. Sample uniform cone (Solid Angle Sampling)
        float r1 = random_float();
        float r2 = random_float();
        
        // cos_theta_max is the angle subtended by the sphere radius
        float cos_theta_max = sqrt(1.0f - radius * radius / dist_squared);
        
        // z is cos(theta) sampled uniformly between [cos_theta_max, 1]
        float z = 1.0f + r2 * (cos_theta_max - 1.0f);
        
        float phi = 2.0f * PI * r1;
        float sin_theta = sqrt(1.0f - z * z);
        float x = cos(phi) * sin_theta;
        float y = sin(phi) * sin_theta;

        // 3. This is the unit direction towards the random point on sphere
        glm::vec3 ray_dir = glm::normalize(x * u + y * v + z * w);

        // 4. Calculate exact distance to the intersection point
        // Ray: O + t * D. Sphere: |P - C|^2 = R^2
        // We know it intersects because we sampled inside the cone.
        // Solves: t^2 + 2(O-C).D * t + (|O-C|^2 - R^2) = 0
        // Simplification: We already have vector (o - center) which is -direction from step 1
        glm::vec3 oc = o - center;
        float b = glm::dot(oc, ray_dir);
        float c = glm::dot(oc, oc) - radius * radius;
        float discriminant = b * b - c;
        
        // We take the smaller positive root (entrance point)
        float t = -b - sqrt(discriminant > 0 ? discriminant : 0);

        return ray_dir * t;
    }
    
    virtual Material* get_material() const override { return mat_ptr.get(); }

public:
    glm::vec3 center;
    float radius;
    std::shared_ptr<Material> mat_ptr;
};