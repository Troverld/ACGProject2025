#pragma once

#include "../core/utils.hpp"
#include <glm/glm.hpp>
#include <memory>

/**
 * @brief Abstract base class for all lights (Point, Area, Directional).
 * Unlike Objects, Lights are sampled explicitly by the integrator.
 */
class Light {
public:
    virtual ~Light() = default;

    /**
     * @brief Sample the light source.
     * Given a point in the scene, this returns a direction towards the light,
     * the radiance arriving from that direction, and the PDF of choosing that direction.
     * 
     * @param origin The point being illuminated (e.g., surface intersection point).
     * @param wi Output: The direction vector from origin TO the light source.
     * @param pdf Output: The probability density of sampling this direction.
     * @param distance Output: Distance to the light point (for shadow ray t_max).
     * @return glm::vec3 The radiance (color * intensity) emitted.
     */
    virtual glm::vec3 sample_li(const glm::vec3& origin, glm::vec3& wi, float& pdf, float& distance) const = 0;
    
    /**
     * @brief Returns the PDF of sampling direction 'wi' from 'origin' towards this light.
     * Used for MIS when a ray hits the light source via BSDF sampling.
     */
    virtual float pdf_value(const glm::vec3& origin, const glm::vec3& wi) const = 0;

    /**
     * @brief emit a photon
     * @param p_pos [out] start point
     * @param p_dir [out] emit direction
     * @param p_power [out] photon energy
     * @param total_photons For energy normalization
     */
    virtual void emit(glm::vec3& p_pos, glm::vec3& p_dir, glm::vec3& p_power, float total_photons) const = 0;
};