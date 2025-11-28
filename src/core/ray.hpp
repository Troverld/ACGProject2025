#pragma once
#include <glm/glm.hpp>

class Ray {
public:
    Ray() {}
    Ray(const glm::vec3& origin, const glm::vec3& direction)
        : orig(origin), dir(glm::normalize(direction)) 
        // 注意：这里我们强制归一化方向，简化后续计算
    {}

    glm::vec3 origin() const { return orig; }
    glm::vec3 direction() const { return dir; }

    // 获取光线在参数 t 处的位置
    glm::vec3 at(float t) const {
        return orig + t * dir;
    }

public:
    glm::vec3 orig;
    glm::vec3 dir;
    // mutable float tm; // 如果做运动模糊需要时间参数，暂时略过
};