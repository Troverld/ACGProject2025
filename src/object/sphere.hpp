#pragma once

#include "object_utils.hpp"
#include "../core/onb.hpp"

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

        // Compute Tangent for Spherical Mapping
        // The tangent should point in the direction of increasing U (around the Y axis).
        // Standard spherical mapping wraps around Y.
        // We use a safe cross product to generate a tangent perpendicular to Normal and Up(0,1,0).
        if (std::abs(outward_normal.y) > 0.999f) {
            // Handle poles: if normal is (0,1,0), tangent is (1,0,0)
            rec.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
        } else {
            rec.tangent = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), outward_normal));
        }
        
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
        if (!this->intersect(Ray(o, v), SHADOW_EPSILON, Infinity, rec)) return 0.0f;

        glm::vec3 direction = center - o;
        float dist_squared = glm::dot(direction, direction);
        float radius_squared = radius * radius;
        
        if (dist_squared <= radius_squared) return 0.0f;
        
        float sin_theta_sq = radius_squared / dist_squared;
        float solid_angle;
        
        // 当角度很小时使用泰勒展开，防止精度丢失
        if (sin_theta_sq < 1e-4f) {
            // 1 - (1 - x/2 - x^2/8) = x/2 + x^2/8
            solid_angle = 2 * PI * (0.5f * sin_theta_sq + 0.125f * sin_theta_sq * sin_theta_sq);
        } else {
            float cos_theta_max = sqrt(1.0f - sin_theta_sq);
            solid_angle = 2 * PI * (1.0f - cos_theta_max);
        }

        if (solid_angle < EPSILON) return 0.0f;

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
        Onb uvw(direction);

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
        glm::vec3 ray_dir = glm::normalize(uvw.local(x, y, z));

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
    
    /**
     * @brief Uniformly samples a point on the sphere surface.
     * Area = 4 * PI * r^2
     */
    virtual void sample_surface(glm::vec3& pos, glm::vec3& normal, float& pdf_area) const override {
        // 1. Generate a random point on a unit sphere (Direction from center)
        glm::vec3 rand_dir = random_unit_vector();
        
        // 2. Compute position: Center + Radius * direction
        pos = center + rand_dir * radius;
        
        // 3. Normal is simply the direction from center
        normal = rand_dir;
        
        // 4. PDF = 1 / Total Area
        float area = 4.0f * PI * radius * radius;
        pdf_area = 1.0f / area;
    }
    
    virtual Material* get_material() const override { return mat_ptr.get(); }

public:
    glm::vec3 center;
    float radius;
    std::shared_ptr<Material> mat_ptr;
};