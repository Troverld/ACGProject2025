#pragma once

#include "../core/utils.hpp"
#include "../scene/object.hpp"
#include "../material/material.hpp" 
#include "../texture/texture.hpp"
#include "../core/onb.hpp"
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

/**
 * @brief Area Light wrapper.
 * This bridges the gap between Geometry (Object) and Lighting (Light).
 * It holds a reference to a geometric object that has an emissive material.
 */
class DiffuseAreaLight : public Light {
public:
    /**
     * @param obj The geometric object that is emitting light.
     */
    DiffuseAreaLight(std::shared_ptr<Object> obj) : shape(obj) {}

    virtual glm::vec3 sample_li(const glm::vec3& origin, glm::vec3& wi, float& pdf, float& distance) const override {
        // 1. Get vector from origin to a random point on the light source
        glm::vec3 to_light_vector = shape->random_pointing_vector(origin);
        
        // 2. Calculate distance
        distance = glm::length(to_light_vector);
        if (distance < EPSILON) {
            pdf = 0;
            return glm::vec3(0);
        }

        // 3. Normalize to get direction
        wi = to_light_vector / distance;

        // 4. Get the PDF of this direction (Solid Angle PDF for Sphere)
        pdf = shape->pdf_value(origin, wi);
        
        if (pdf <= EPSILON) return glm::vec3(0.0f);

        // 5. Get emitted radiance
        // Note: For Area lights, emission is usually directional (cosine weighted at the source),
        // but DiffuseLight material simplifies this to uniform emission.
        // We supply the actual hit point (origin + wi * distance) to support spatially varying emission textures.
        return shape->get_material()->emitted(0, 0, origin + wi * distance); 
    }    
    
    /**
     * @brief Delegate PDF calculation to the underlying shape.
     */
    virtual float pdf_value(const glm::vec3& origin, const glm::vec3& wi) const override {
        return shape->pdf_value(origin, wi);
    }

    virtual void emit(glm::vec3& p_pos, glm::vec3& p_dir, glm::vec3& p_power, float total_photons) const override {
        glm::vec3 normal;
        float pdf_area;
        
        // Calculate initial power estimate for one photon.
        // Flux (Phi) = Le * Area * PI (for Lambertian emitter)
        // Power_photon = Phi / N_emitted
        // Note: We don't compute exact area here, we rely on the Monte Carlo estimator during emission.
        // Estimator: Power = (Le * PI) / (pdf_area * N_emitted)
        // The cos(theta) term in Flux integral cancels out with Cosine Weighted sampling PDF (cos/PI).

        shape->sample_surface(p_pos, normal, pdf_area);

        Onb uvw(normal);
        p_dir = uvw.local(random_cosine_direction());

        glm::vec3 Le = shape->get_material()->emitted(0, 0, p_pos);
        
        if (pdf_area < EPSILON) {
            p_power = glm::vec3(0.0f);
        } else {
            p_power = (Le * PI) / (pdf_area * total_photons);
        }
    }

public:
    std::shared_ptr<Object> shape;
};

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