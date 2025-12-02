#pragma once
#include "texture.hpp"
#include "solid_color.hpp"
#include <memory>
#include <cmath>

/**
 * @brief A procedural checkerboard texture.
 * Great for debugging UV mappings and spatial consistency.
 */
class CheckerTexture : public Texture {
public:
    CheckerTexture() {}

    /**
     * @brief Creates a checkerboard from two other textures.
     * 
     * @param _even Texture for even tiles.
     * @param _odd Texture for odd tiles.
     * @param _scale Control the density of the pattern.
     */
    CheckerTexture(std::shared_ptr<Texture> _even, std::shared_ptr<Texture> _odd, float _scale = 10.0f)
        : even(_even), odd(_odd), scale(_scale) {}

    /**
     * @brief Creates a checkerboard from two colors.
     */
    CheckerTexture(glm::vec3 c1, glm::vec3 c2, float _scale = 10.0f)
        : even(std::make_shared<SolidColor>(c1)), odd(std::make_shared<SolidColor>(c2)), scale(_scale) {}

    virtual glm::vec3 value(float u, float v, const glm::vec3& p) const override {
        // Use spatial coordinates for 3D checkerboard
        float sines = sin(scale * p.x) * sin(scale * p.y) * sin(scale * p.z);
        
        if (sines < 0)
            return odd->value(u, v, p);
        else
            return even->value(u, v, p);
    }

public:
    std::shared_ptr<Texture> even;
    std::shared_ptr<Texture> odd;
    float scale;
};