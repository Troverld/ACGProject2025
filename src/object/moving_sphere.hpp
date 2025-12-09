#pragma once

#include "object_utils.hpp"

class MovingSphere : public Object {
public:
    MovingSphere() {}
    MovingSphere(glm::vec3 cen0, glm::vec3 cen1, float _time0, float _time1, float r, std::shared_ptr<Material> m)
        : center0(cen0), center1(cen1), time0(_time0), time1(_time1), radius(r), mat_ptr(m)
    {};

    virtual bool intersect(const Ray& r, float t_min, float t_max, HitRecord& rec) const override {
        glm::vec3 center = center_at(r.time());
        glm::vec3 oc = r.origin() - center;
        float a = glm::dot(r.direction(), r.direction());
        float half_b = glm::dot(oc, r.direction());
        float c = glm::dot(oc, oc) - radius * radius;

        float discriminant = half_b * half_b - a * c;
        if (discriminant < 0) return false;
        float sqrtd = sqrt(discriminant);

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
        rec.mat_ptr = mat_ptr.get();
        rec.object = this;
        
        // UV mapping usually similar to static sphere
        // get_sphere_uv(outward_normal, rec.u, rec.v); 

        return true;
    }

    virtual void sample_surface(glm::vec3& pos, glm::vec3& normal, float& area) const override {
        // 1. Generate a random point on a unit sphere (Direction from center)
        glm::vec3 rand_dir = random_unit_vector();
        float time = random_float(time0, time1);
        glm::vec3 center = center_at(time);
        // 2. Compute position: Center + Radius * direction
        pos = center + rand_dir * radius;
        
        // 3. Normal is simply the direction from center
        normal = rand_dir;
        
        // 4. PDF = 1 / Total Area
        area = 4.0f * PI * radius * radius;
    }
    
    virtual glm::vec3 random_pointing_vector(const glm::vec3& origin) const override{return glm::vec3(0.0f);}
    virtual float pdf_value(const glm::vec3& origin, const glm::vec3& wi) const override {return 0.0f;}

    virtual bool bounding_box(float _t0, float _t1, AABB& output_box) const override {
        AABB box0(
            center_at(_t0) - glm::vec3(radius),
            center_at(_t0) + glm::vec3(radius));
        AABB box1(
            center_at(_t1) - glm::vec3(radius),
            center_at(_t1) + glm::vec3(radius));
        output_box = surrounding_box(box0, box1);
        return true;
    }

    glm::vec3 center_at(float time) const {
        return center0 + ((time - time0) / (time1 - time0)) * (center1 - center0);
    }

    virtual Material* get_material() const override { return mat_ptr.get(); }

public:
    glm::vec3 center0, center1;
    float time0, time1;
    float radius;
    std::shared_ptr<Material> mat_ptr;
};