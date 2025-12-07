#pragma once
#include <glm/glm.hpp>
#include <string>

/**
 * @brief Abstract base class for textures.
 * A texture maps a point in 2D (u,v) or 3D (p) space to a color value.
 */
class Texture {
public:
    virtual ~Texture() = default;

    /**
     * @brief Samples the color at a given coordinate.
     * 
     * @param u Texture coordinate U [0, 1].
     * @param v Texture coordinate V [0, 1].
     * @param p Point in 3D space (used for solid/procedural textures).
     * @return glm::vec3 The color value (radiance/albedo).
     */
    virtual glm::vec3 value(float u, float v, const glm::vec3& p) const = 0;
};