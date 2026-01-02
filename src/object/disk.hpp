#pragma once

#include "object_utils.hpp"
#include "../core/onb.hpp"

/**
 * @brief A Disk object defined by a center, a normal, and a radius.
 */
class Disk : public Object {
public:
    Disk() {}

    /**
     * @brief Construct a new Disk.
     * 
     * @param c Center position.
     * @param n Normal vector (orientation).
     * @param r Radius.
     * @param m Material.
     */
    Disk(glm::vec3 c, glm::vec3 n, float r, std::shared_ptr<Material> m)
        : center(c), normal(glm::normalize(n)), radius(r), mat_ptr(m) {}

    /**
     * @brief Ray-Disk intersection.
     * 1. Intersect ray with the plane defined by center and normal.
     * 2. Check if the distance from intersection to center is <= radius.
     */
    virtual bool intersect(const Ray& r, float t_min, float t_max, HitRecord& rec) const override {
        float denom = glm::dot(normal, r.direction());

        // Ray is parallel to the plane
        if (std::abs(denom) < 1e-6f) return false;

        float t = glm::dot(center - r.origin(), normal) / denom;

        if (t < t_min || t > t_max) return false;

        glm::vec3 p = r.at(t);
        glm::vec3 v = p - center;
        float dist_squared = glm::dot(v, v);

        // Check if point is within the disk radius
        if (dist_squared > radius * radius) return false;

        rec.t = t;
        rec.p = p;
        
        // Ensure normal points against the ray
        rec.set_face_normal(r, normal);

        // Compute UV coordinates (Polar mapping)
        // u = r / radius (0 to 1 from center to edge)
        // v = angle / 2PI (0 to 1 around the circle)
        // We need a local basis to compute the angle
        Onb uvw(normal); 
        // Project vector v onto the local basis (uvw.u() and uvw.v() correspond to local X and Y)
        float x = glm::dot(v, uvw.u());
        float y = glm::dot(v, uvw.v());
        
        rec.u = sqrt(dist_squared) / radius; 
        float phi = atan2(y, x);
        if (phi < 0) phi += 2 * PI;
        rec.v = phi / (2 * PI);

        // Tangent for normal mapping (local X axis)
        rec.tangent = uvw.u();

        rec.mat_ptr = mat_ptr.get();
        rec.object = this;

        return true;
    }

    /**
     * @brief Bounding box for a disk.
     * Since a disk is flat, we must give the box a small thickness (EPSILON) 
     * to prevent BVH issues when the disk is axis-aligned.
     */
    virtual bool bounding_box(float time0, float time1, AABB& output_box) const override {
        // Calculate the extent of the disk in X, Y, Z axes
        // Extent in direction axis i is: R * sqrt(1 - (n_i)^2)
        // But a simpler safe approach is to clamp ranges.
        
        // Exact method using component-wise radius projection:
        glm::vec3 e = glm::vec3(
            radius * sqrt(1.0f - normal.x * normal.x),
            radius * sqrt(1.0f - normal.y * normal.y),
            radius * sqrt(1.0f - normal.z * normal.z)
        );

        output_box = AABB(
            center - e - glm::vec3(EPSILON), // Add epsilon padding
            center + e + glm::vec3(EPSILON)
        );
        return true;
    }

    /**
     * @brief PDF of sampling a direction towards this disk.
     * PDF(direction) = distance^2 / (Area * |cos(theta)|)
     */
    virtual float pdf_value(const glm::vec3& o, const glm::vec3& v) const override {
        HitRecord rec;
        if (!this->intersect(Ray(o, v), 0.001f, Infinity, rec))
            return 0.0f;

        float area = PI * radius * radius;
        float distance_squared = rec.t * rec.t * glm::dot(v, v);
        float cosine = std::abs(glm::dot(v, rec.normal) / glm::length(v));
        
        if (cosine < EPSILON) return 0.0f;

        return distance_squared / (cosine * area);
    }

    /**
     * @brief Randomly points to a spot on the disk from origin.
     */
    virtual glm::vec3 random_pointing_vector(const glm::vec3& o) const override {
        // 1. Sample a point uniformly on a unit disk on XY plane
        float r1 = random_float(); // For angle
        float r2 = random_float(); // For radius
        
        float r = radius * sqrt(r2); // Sqrt for uniform area sampling
        float phi = 2 * PI * r1;
        
        float x = r * cos(phi);
        float y = r * sin(phi);

        // 2. Transform to world space using ONB aligned with disk normal
        Onb uvw(normal);
        glm::vec3 random_point_on_disk = center + uvw.local(x, y, 0.0f);

        // 3. Return vector from origin to that point
        return random_point_on_disk - o;
    }

    /**
     * @brief Uniformly sample the surface of the disk.
     */
    virtual void sample_surface(glm::vec3& pos, glm::vec3& sample_normal, float& area) const override {
        float r1 = random_float();
        float r2 = random_float();

        float r = radius * sqrt(r2);
        float phi = 2 * PI * r1;

        float x = r * cos(phi);
        float y = r * sin(phi);

        Onb uvw(normal);
        pos = center + uvw.local(x, y, 0.0f);
        sample_normal = normal;
        area = PI * radius * radius;
    }

    virtual Material* get_material() const override { return mat_ptr.get(); }

public:
    glm::vec3 center;
    glm::vec3 normal;
    float radius;
    std::shared_ptr<Material> mat_ptr;
};