#pragma once
#include "texture_utils.hpp"

/**
 * @brief A texture that returns a constant color regardless of coordinates.
 * Adapts a simple color to the Texture interface.
 */
class SolidColor : public Texture {
public:
    SolidColor() {}
    
    /**
     * @brief Construct a SolidColor from a vector.
     */
    SolidColor(const glm::vec3& c) : color_value(c) {}

    /**
     * @brief Construct a SolidColor from r, g, b components.
     */
    SolidColor(float r, float g, float b) : SolidColor(glm::vec3(r, g, b)) {}

    virtual glm::vec3 value(float u, float v, const glm::vec3& p) const override {
        return color_value;
    }

private:
    glm::vec3 color_value;
};