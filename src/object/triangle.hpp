#pragma once

#include "object_utils.hpp"

/**
 * @brief Triangle primitive.
 * Supports UV mapping and Optional Smooth Shading (Vertex Normals).
 */
class Triangle : public Object {
public:
    /**
     * @brief Construct a Triangle (Flat Shading).
     * Calculates geometric normal automatically.
     */
    Triangle(glm::vec3 _v0, glm::vec3 _v1, glm::vec3 _v2, std::shared_ptr<Material> m,
             glm::vec2 _uv0 = glm::vec2(0,0), glm::vec2 _uv1 = glm::vec2(1,0), glm::vec2 _uv2 = glm::vec2(0,1))
        : v0(_v0), v1(_v1), v2(_v2), uv0(_uv0), uv1(_uv1), uv2(_uv2), mat_ptr(m), use_vertex_normals(false)
    {
        init();
        // For flat shading, vertex normals are the same as face normal
        n0 = n1 = n2 = face_normal;
    }

    /**
     * @brief Construct a Triangle (Smooth Shading).
     * Uses provided vertex normals for interpolation.
     */
    Triangle(glm::vec3 _v0, glm::vec3 _v1, glm::vec3 _v2, 
             glm::vec3 _n0, glm::vec3 _n1, glm::vec3 _n2,
             std::shared_ptr<Material> m,
             glm::vec2 _uv0 = glm::vec2(0,0), glm::vec2 _uv1 = glm::vec2(1,0), glm::vec2 _uv2 = glm::vec2(0,1))
        : v0(_v0), v1(_v1), v2(_v2), 
          n0(_n0), n1(_n1), n2(_n2),
          uv0(_uv0), uv1(_uv1), uv2(_uv2), mat_ptr(m), use_vertex_normals(true)
    {
        init();
    }

    /**
     * @brief Common initialization logic.
     */
    void init() {
        glm::vec3 edge1 = v1 - v0;
        glm::vec3 edge2 = v2 - v0;
        face_normal = glm::normalize(glm::cross(edge1, edge2));
        area = 0.5f * glm::length(glm::cross(edge1, edge2));

        // Tangent calculation for Normal Mapping
        glm::vec2 delta_uv1 = uv1 - uv0;
        glm::vec2 delta_uv2 = uv2 - uv0;
        float f = 1.0f / (delta_uv1.x * delta_uv2.y - delta_uv2.x * delta_uv1.y + EPSILON);

        tangent.x = f * (delta_uv2.y * edge1.x - delta_uv1.y * edge2.x);
        tangent.y = f * (delta_uv2.y * edge1.y - delta_uv1.y * edge2.y);
        tangent.z = f * (delta_uv2.y * edge1.z - delta_uv1.y * edge2.z);
        tangent = glm::normalize(tangent);
    }

    /**
     * @brief Möller–Trumbore intersection algorithm.
     */
    virtual bool intersect(const Ray& r, float t_min, float t_max, HitRecord& rec) const override {
        glm::vec3 edge1 = v1 - v0;
        glm::vec3 edge2 = v2 - v0;
        glm::vec3 pvec = glm::cross(r.direction(), edge2);
        
        float det = glm::dot(edge1, pvec);
        if (std::abs(det) < EPSILON) return false; // Culling or degenerate
        
        float inv_det = 1.0f / det;
        glm::vec3 tvec = r.origin() - v0;
        
        float u = glm::dot(tvec, pvec) * inv_det;
        if (u < 0.0f || u > 1.0f) return false;

        glm::vec3 qvec = glm::cross(tvec, edge1);
        float v = glm::dot(r.direction(), qvec) * inv_det;
        if (v < 0.0f || u + v > 1.0f) return false;

        float t = glm::dot(edge2, qvec) * inv_det;
        if (t < t_min || t > t_max) return false;

        rec.t = t;
        rec.p = r.at(t);
        
        // --- Normal Interpolation ---
        // Barycentric weights: w, u, v
        // P = w*v0 + u*v1 + v*v2
        float w = 1.0f - u - v;

        glm::vec3 shading_normal;
        if (use_vertex_normals) {
            shading_normal = glm::normalize(w * n0 + u * n1 + v * n2);
        } else {
            shading_normal = face_normal;
        }

        rec.set_face_normal(r, shading_normal);
        
        // Interpolate UV
        rec.u = w * uv0.x + u * uv1.x + v * uv2.x;
        rec.v = w * uv0.y + u * uv1.y + v * uv2.y;
        
        rec.tangent = tangent;
        rec.mat_ptr = mat_ptr.get();
        rec.object = this;

        return true;
    }

    virtual bool bounding_box(float time0, float time1, AABB& output_box) const override {
        float min_x = std::min({v0.x, v1.x, v2.x});
        float min_y = std::min({v0.y, v1.y, v2.y});
        float min_z = std::min({v0.z, v1.z, v2.z});

        float max_x = std::max({v0.x, v1.x, v2.x});
        float max_y = std::max({v0.y, v1.y, v2.y});
        float max_z = std::max({v0.z, v1.z, v2.z});

        output_box = AABB(
            glm::vec3(min_x, min_y, min_z) - glm::vec3(PADDING_EPSILON), 
            glm::vec3(max_x, max_y, max_z) + glm::vec3(PADDING_EPSILON)
        );
        return true;
    }

    /**
     * @brief PDF for importance sampling this triangle (Area Light).
     * pdf = distance_squared / (area * cos_theta)
     */
    virtual float pdf_value(const glm::vec3& origin, const glm::vec3& v) const override {
        HitRecord rec;
        if (!this->intersect(Ray(origin, v), 0.001f, Infinity, rec))
            return 0.0f;
        float distance_squared = rec.t * rec.t * glm::dot(v, v);
        float cosine = std::abs(glm::dot(v, rec.normal) / glm::length(v));
        return distance_squared / (area * cosine);
    }

    /**
     * @brief Randomly sample a point on the triangle surface.
     */
    virtual glm::vec3 random_pointing_vector(const glm::vec3& origin) const override {
        float sqrt_r1 = sqrt(random_float());
        float r2 = random_float();
        float u = 1.0f - sqrt_r1;
        float v = r2 * sqrt_r1;
        glm::vec3 random_point = (1.0f - u - v) * v0 + u * v1 + v * v2;
        return random_point - origin;
    }

    /**
     * @brief Uniformly samples a point on the triangle surface using Barycentric coordinates.
     * Area is precomputed.
     */
    virtual void sample_surface(glm::vec3& pos, glm::vec3& normal, float& area) const override {
        float sqrt_r1 = sqrt(random_float());
        float r2 = random_float();
        float u = 1.0f - sqrt_r1;
        float v = r2 * sqrt_r1;
        
        pos = (1.0f - u - v) * v0 + u * v1 + v * v2;
        // Use face normal for sampling area light direction for consistency
        normal = face_normal; 
        area = this->area;
    }

    virtual Material* get_material() const override { return mat_ptr.get(); }

public:
    glm::vec3 v0, v1, v2;
    glm::vec3 n0, n1, n2; // Vertex Normals
    glm::vec2 uv0, uv1, uv2;
    glm::vec3 face_normal;
    glm::vec3 tangent;
    float area;
    std::shared_ptr<Material> mat_ptr;
    bool use_vertex_normals;
};