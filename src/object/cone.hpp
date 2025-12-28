#pragma once

#include "object_utils.hpp"
#include "../core/onb.hpp"

/**
 * @brief A Cone object aligned with the Y-axis.
 * Defined by a base center, base radius, and height.
 * The cone tapers from the base radius at 'center.y' to a point at 'center.y + height'.
 */
class Cone : public Object {
public:
    Cone() {}

    /**
     * @brief Construct a new Cone object.
     * 
     * @param cen Base center of the cone.
     * @param r Base radius.
     * @param h Height (extends from center.y to center.y + h).
     * @param m Material pointer.
     */
    Cone(glm::vec3 cen, float r, float h, std::shared_ptr<Material> m)
        : center(cen), radius(r), height(h), mat_ptr(m) {
        
        // Precompute side area calculation constants
        float slant_height = sqrt(radius * radius + height * height);
        float side_area = PI * radius * slant_height;
        float base_area = PI * radius * radius;
        area = side_area + base_area;
        
        // Probability to sample base cap
        base_area_ratio = base_area / area;
    };

    virtual bool intersect(const Ray& r, float t_min, float t_max, HitRecord& rec) const override {
        // Transform ray to local space (relative to base center)
        glm::vec3 o = r.origin() - center;
        glm::vec3 d = r.direction();

        // Constants
        double k = radius / height;
        double k_sq = k * k;
        double h = height;

        // 1. Intersect with Conical Surface
        // Equation: x^2 + z^2 = (k * (h - y))^2
        // x^2 + z^2 - k^2*(h-y)^2 = 0
        
        double A = d.x * d.x + d.z * d.z - k_sq * d.y * d.y;
        double B = 2.0 * (o.x * d.x + o.z * d.z - k_sq * (o.y - h) * d.y);
        double C = o.x * o.x + o.z * o.z - k_sq * (o.y - h) * (o.y - h);

        double t_hit = t_max;
        bool hit_side = false;
        bool hit_base = false;

        // Solve Quadratic
        if (std::abs(A) > EPSILON * EPSILON) {
            double discriminant = B * B - 4 * A * C;
            if (discriminant >= 0) {
                double sqrt_d = sqrt(discriminant);
                double t1 = (-B - sqrt_d) / (2 * A);
                double t2 = (-B + sqrt_d) / (2 * A);

                // Check t1
                if (t1 > t_min && t1 < t_hit) {
                    double y = o.y + t1 * d.y;
                    if (y >= 0 && y <= height) {
                        t_hit = t1;
                        hit_side = true;
                    }
                }
                // Check t2
                if (t2 > t_min && t2 < t_hit) {
                    double y = o.y + t2 * d.y;
                    if (y >= 0 && y <= height) {
                        t_hit = t2;
                        hit_side = true;
                    }
                }
            }
        } else {
            // Ray parallel to side (rare), treat as linear if needed or ignore
        }

        // 2. Intersect with Base Cap (y = 0)
        // Ray: o.y + t * d.y = 0  =>  t = -o.y / d.y
        if (std::abs(d.y) > 1e-8) {
            double t_base = -o.y / d.y;
            if (t_base > t_min && t_base < t_hit) {
                double x = o.x + t_base * d.x;
                double z = o.z + t_base * d.z;
                if (x * x + z * z <= radius * radius) {
                    t_hit = t_base;
                    hit_base = true;
                    hit_side = false; // Base is closer
                }
            }
        }

        // Finalize Intersection
        if (t_hit >= t_max) return false;

        rec.t = static_cast<float>(t_hit);
        rec.p = r.at(rec.t);
        rec.mat_ptr = mat_ptr.get();
        rec.object = this;

        glm::vec3 local_p = rec.p - center;

        if (hit_base) {
            // Normal is down
            rec.set_face_normal(r, glm::vec3(0, -1, 0));
            // Planar mapping for UV
            rec.u = (local_p.x / radius + 1.f) * 0.5f;
            rec.v = (local_p.z / radius + 1.f) * 0.5f;
            rec.tangent = glm::vec3(1, 0, 0);
        } 
        else if (hit_side) {
            // Normal calculation: Gradient of Implicit Function
            // N = (x, k*radius_at_y, z) simplified vector logic
            // The normal is perpendicular to the slope.
            // Horizontal component len: 1, Vertical component len: k
            // Slope vector (in cross section) is (1, -1/k). Normal is (1/k, 1).
            // Actually, simplest is:
            float r_at_y = (height - local_p.y) * (radius / height);
            glm::vec3 outward_normal(local_p.x, 0, local_p.z);
            outward_normal = glm::normalize(outward_normal); // Horizontal direction
            // Tilt it up. The tangent slope is -height/radius. Normal slope is radius/height.
            outward_normal.x *= height;
            outward_normal.z *= height;
            outward_normal.y = radius; 
            
            rec.set_face_normal(r, glm::normalize(outward_normal));
            
            // Cylindrical mapping
            float phi = atan2(local_p.z, local_p.x);
            if (phi < 0) phi += 2 * PI;
            rec.u = phi / (2 * PI);
            rec.v = local_p.y / height;
            
            // Tangent along U direction
            rec.tangent = glm::normalize(glm::cross(glm::vec3(0,1,0), rec.normal));
        }

        return true;
    }

    virtual bool bounding_box(float time0, float time1, AABB& output_box) const override {
        // Box contains the base disk and the tip point
        glm::vec3 min_p = center - glm::vec3(radius, 0, radius);
        glm::vec3 max_p = center + glm::vec3(radius, height, radius);
        output_box = AABB(min_p, max_p);
        return true;
    }

    virtual float pdf_value(const glm::vec3& o, const glm::vec3& v) const override {
        HitRecord rec;
        if (!this->intersect(Ray(o, v), 0.001f, Infinity, rec))
            return 0.0f;
        
        // Convert Area PDF to Solid Angle PDF
        // p(omega) = p(area) * dist^2 / cos(theta)
        // p(area) = 1 / TotalArea
        
        float dist_squared = rec.t * rec.t * glm::dot(v, v);
        float cosine = std::abs(glm::dot(v, rec.normal)); // v is ray direction, normalized? intersect uses ray direction
        
        // Note: Object interface implies v is direction. If it's not normalized, we need to handle that.
        // Assuming v is normalized direction from random_pointing_vector logic.
        
        if (cosine < 1e-4f) return 0.0f;

        return dist_squared / (cosine * area);
    }

    virtual glm::vec3 random_pointing_vector(const glm::vec3& o) const override {
        // Sample a point on the cone surface
        glm::vec3 rand_p, rand_n;
        float dummy_area;
        sample_surface(rand_p, rand_n, dummy_area);

        // Return vector from origin to surface point
        return rand_p - o;
    }

    virtual void sample_surface(glm::vec3& pos, glm::vec3& normal, float& area_out) const override {
        area_out = area;
        
        // Choose between base and side
        if (random_float() < base_area_ratio) {
            // Sample Base (Disk)
            glm::vec3 point_on_disk = random_in_unit_disk() * radius;
            // random_in_unit_disk usually returns Z=0. We map to X,Z plane.
            pos = center + glm::vec3(point_on_disk.x, 0.0f, point_on_disk.y);
            normal = glm::vec3(0, -1, 0);
        } else {
            // Sample Side
            // To sample uniformly on the conical surface:
            // The radius at height y (measured from tip down) is r(y) = (y/H)*R.
            // Circumference C(y) propto y. Area element dA propto y dy.
            // PDF(y) ~ y. CDF(y) ~ y^2. 
            // Inverse CDF: y = sqrt(xi).
            
            float r1 = random_float();
            float r2 = random_float();
            
            // h_sample is distance from TIP
            float h_sample = sqrt(r1) * height; 
            
            // Actual y in local coords (from base)
            float y_local = height - h_sample;
            
            float phi = 2 * PI * r2;
            float r_at_y = (h_sample / height) * radius;
            
            float x_local = cos(phi) * r_at_y;
            float z_local = sin(phi) * r_at_y;
            
            pos = center + glm::vec3(x_local, y_local, z_local);
            
            // Compute Normal at this point
            // Slope vector in cross-section: (1, height/radius). Normal: (height, radius).
            glm::vec3 n = glm::vec3(x_local, 0, z_local);
            if (glm::length(n) > 0) n = glm::normalize(n);
            
            n.x *= height;
            n.z *= height;
            n.y = radius;
            normal = glm::normalize(n);
        }
    }

    virtual Material* get_material() const override { return mat_ptr.get(); }

public:
    glm::vec3 center; // Center of the base
    float radius;
    float height;
    std::shared_ptr<Material> mat_ptr;
    
private:
    float area;
    float base_area_ratio;
};