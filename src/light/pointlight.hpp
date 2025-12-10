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
        : position(pos), intensity(i) {
            this -> est_power = grayscale(i) * 4.0f * PI;
            // if(this -> est_power < EPSILON) this -> est_power = EPSILON;
        }

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
        p_dir = random_unit_vector();

        // Total Flux = 4 * PI * Intensity
        // Power per photon = Flux / N
        p_power = (intensity * 4.0f * PI) / total_photons;
    }

    virtual bool emit_targeted(
        glm::vec3& p_pos, glm::vec3& p_dir, glm::vec3& p_power, float total_photons, const Object& target
    ) const override {
        p_pos = position;
        glm::vec3 vec_to_target = target.random_pointing_vector(p_pos);
        float dist = glm::length(vec_to_target);
        if (dist <= EPSILON) return false;
        p_dir = vec_to_target / dist;
        float pdf_dir = target.pdf_value(p_pos, p_dir);
        if (pdf_dir <= EPSILON) return false;
        // Power_new = Power_old * Weight 
        //           = (Intensity * 4PI / N) * (1 / (4PI * target_pdf))
        //           = Intensity / (N * target_pdf)
        p_power = intensity  / (total_photons * pdf_dir);
        return true;
    }

public:
    glm::vec3 position;
    glm::vec3 intensity;
};