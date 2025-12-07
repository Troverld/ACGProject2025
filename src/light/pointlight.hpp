#pragma once

#include "light_utils.hpp"

/**
 * @brief Point Light Source (Singularity).
 * Represents an infinitely small point emitting light in all directions.
 * 
 * Note: Since it has no geometry, it cannot be hit by scene intersection rays.
 * It is invisible to the camera and contributes lighting ONLY via 
 * Next Event Estimation (sample_li).
 */
class PointLight : public Light {
public:
    /**
     * @param pos World space position.
     * @param i Radiant Intensity (Flux per solid angle, W/sr).
     */
    PointLight(const glm::vec3& pos, const glm::vec3& i) 
        : position(pos), intensity(i) {}

    virtual glm::vec3 sample_li(const glm::vec3& origin, glm::vec3& wi, float& pdf, float& distance) const override {
        glm::vec3 d = position - origin;
        float dist_sq = glm::dot(d, d);
        distance = std::sqrt(dist_sq);

        if (distance < EPSILON) {
            pdf = 0.0f;
            return glm::vec3(0.0f);
        }

        wi = d / distance;
        
        // Since a point light is a singularity, if we select it, the direction is deterministic.
        // The PDF relative to the light sampling domain is 1.0 (discrete choice).
        pdf = 1.0f; 

        // Attenuation: Inverse Square Law (I / r^2)
        return intensity / dist_sq;
    }

    /**
     * @brief PDF of hitting this light via random BSDF sampling.
     * Always 0 because the probability of a random ray hitting a geometric point is zero.
     */
    virtual float pdf_value(const glm::vec3& origin, const glm::vec3& wi) const override {
        return 0.0f;
    }

    virtual void emit(glm::vec3& p_pos, glm::vec3& p_dir, glm::vec3& p_power, float total_photons) const override {
        p_pos = position;
        
        // Uniform Spherical Sampling
        float u1 = random_float();
        float u2 = random_float();
        float z = 1.0f - 2.0f * u1;
        float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
        float phi = 2.0f * PI * u2;
        p_dir = glm::vec3(r * cos(phi), r * sin(phi), z);

        // Total Flux = 4 * PI * Intensity
        // Power per photon = Flux / N
        p_power = (intensity * 4.0f * PI) / total_photons;
    }

public:
    glm::vec3 position;
    glm::vec3 intensity;
};