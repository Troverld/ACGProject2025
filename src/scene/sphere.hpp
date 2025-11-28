#pragma once

#include "object.hpp"
#include <glm/glm.hpp>

class Sphere : public Object {
public:
    Sphere() {}
    Sphere(glm::vec3 cen, float r) : center(cen), radius(r) {};

    virtual bool intersect(const Ray& r, float t_min, float t_max, HitRecord& rec) const override {
        glm::vec3 oc = r.origin() - center;
        // 求解一元二次方程: at^2 + bt + c = 0
        // a = dot(dir, dir) = 1 (因为我们在Ray里归一化了)
        // b = 2 * dot(oc, dir)
        // c = dot(oc, oc) - r^2
        
        float a = glm::dot(r.direction(), r.direction());
        float half_b = glm::dot(oc, r.direction());
        float c = glm::dot(oc, oc) - radius * radius;
        
        float discriminant = half_b * half_b - a * c;
        if (discriminant < 0) return false;
        
        float sqrtd = sqrt(discriminant);

        // 寻找最近的合法根
        float root = (-half_b - sqrtd) / a;
        if (root < t_min || root > t_max) {
            root = (-half_b + sqrtd) / a;
            if (root < t_min || root > t_max)
                return false;
        }

        // 记录交点信息
        rec.t = root;
        rec.p = r.at(rec.t);
        glm::vec3 outward_normal = (rec.p - center) / radius; // 归一化法线
        rec.set_face_normal(r, outward_normal);

        return true;
    }

public:
    glm::vec3 center;
    float radius;
};