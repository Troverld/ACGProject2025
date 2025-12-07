#pragma once

#include "../texture/texture_utils.hpp"
#include "light_utils.hpp"
/**
 * @brief Infinite Area Light (Environment Light).
 * Represents a distant light source surrounding the scene (e.g., HDRI).
 * Samples are generated uniformly on the unit sphere.
 */
class EnvironmentLight : public Light {
public:
    /**
     * @brief Construct a new Environment Light object.
     * @param tex The background texture (HDRI or solid color).
     */
    EnvironmentLight(std::shared_ptr<Texture> tex) : texture(tex) {}

    /**
     * @brief Samples a direction from the environment.
     * Currently uses Uniform Spherical Sampling.
     * 
     * @param origin Not used for infinite lights (direction is global).
     * @param wi Output: Direction towards the light.
     * @param pdf Output: PDF of sampling this direction (1/4pi for uniform).
     * @param distance Output: Distance to light (Infinity).
     * @return glm::vec3 Radiance from the environment in direction wi.
     */
    virtual glm::vec3 sample_li(const glm::vec3& origin, glm::vec3& wi, float& pdf, float& distance) const override {
        // Uniform sampling on sphere
        float u1 = random_float();
        float u2 = random_float();
        float z = 1.0f - 2.0f * u1;
        float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
        float phi = 2.0f * PI * u2;
        
        wi = glm::vec3(r * cos(phi), r * sin(phi), z);
        
        distance = Infinity;
        pdf = 1.0f / (4.0f * PI); // Uniform PDF over sphere
        
        return eval(wi);
    }

    /**
     * @brief PDF of sampling a specific direction.
     * For Uniform Spherical Sampling, this is constant.
     */
    virtual float pdf_value(const glm::vec3& origin, const glm::vec3& wi) const override {
        return 1.0f / (4.0f * PI);
    }

    virtual void emit(glm::vec3& p_pos, glm::vec3& p_dir, glm::vec3& p_power, float total_photons) const override {p_power = glm::vec3(0.0f);}

    /**
     * @brief Evaluate the radiance from the environment in a given direction.
     * Useful for rays that miss all geometry.
     * 
     * @param dir The direction to evaluate.
     * @return glm::vec3 The radiance/color from the environment in that direction.
     */
    glm::vec3 eval(const glm::vec3& dir) const {
        float u, v;
        glm::vec3 unit_dir = glm::normalize(dir);
        get_sphere_uv(unit_dir, u, v);
        return texture->value(u, v, unit_dir);
    }

public:
    std::shared_ptr<Texture> texture;
};