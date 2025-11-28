#pragma once

#include "../core/ray.hpp"
#include <glm/glm.hpp>

struct HitRecord {
    glm::vec3 p;        // 交点位置
    glm::vec3 normal;   // 交点法线
    float t;            // 光线参数 t
    bool front_face;    // 光线是从外部射入还是内部射出

    // 确保法线始终指向光线入射的相反方向（即指向外部）
    // 如果是从内部射出，法线需要反转
    void set_face_normal(const Ray& r, const glm::vec3& outward_normal) {
        front_face = glm::dot(r.direction(), outward_normal) < 0;
        normal = front_face ? outward_normal : -outward_normal;
    }
};

class Object {
public:
    virtual ~Object() = default;

    // 纯虚函数：求交
    // t_min 和 t_max 用于限制有效交点范围（例如剔除在该物体后面的交点）
    virtual bool intersect(const Ray& r, float t_min, float t_max, HitRecord& rec) const = 0;
};