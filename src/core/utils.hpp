#pragma once

#include <cmath>
#include <limits>
#include <memory>
#include <random>
#include <algorithm> // for std::clamp

// Common Headers
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp> // 包含 GLM 的常量，如 pi

// Constants
const float Infinity = std::numeric_limits<float>::infinity();
// 使用 GLM 的 PI，或者保持自定义简化书写
const float PI = glm::pi<float>();

// Random Number Generation
// 随机数生成还是需要封装一下，标准库的 random 稍微有点繁琐
inline float random_float() {
    static thread_local std::mt19937 generator;
    std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
    return distribution(generator);
}

inline float random_float(float min, float max) {
    return min + (max - min) * random_float();
}

// 既然有了 std::clamp 和 glm::radians，这里就不需要手动定义了
// 我们保留 Utils.hpp 主要为了集中管理随机数和常量