#pragma once

#include "../object/object_utils.hpp"
#include "../material/material_utils.hpp" 
#include "../core/onb.hpp"
#include "light_utils.hpp"

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
    DiffuseAreaLight(std::shared_ptr<Object> obj) : shape(obj) {
        glm::vec3 pos, normal; float area;
        shape->sample_surface(pos, normal, area);

        glm::vec3 accum_emit(0.0f);
        const int samples = 8;
        for(int i=0; i<samples; ++i) {
            shape->sample_surface(pos, normal, area);
            accum_emit += shape->get_material()->emitted(0, 0, pos);
        }
        glm::vec3 avg_emit = accum_emit / float(samples);
        this -> est_power = grayscale(avg_emit) * area * PI;
        // if(this -> est_power < EPSILON) this -> est_power = EPSILON;
    }

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
        float area;
        
        // Calculate initial power estimate for one photon.
        // Flux (Phi) = Le * Area * PI (for Lambertian emitter)
        // Power_photon = Phi / N_emitted
        // The cos(theta) term in Flux integral cancels out with Cosine Weighted sampling PDF (cos/PI).

        shape->sample_surface(p_pos, normal, area);

        Onb uvw(normal);
        p_dir = uvw.local(random_cosine_direction());

        glm::vec3 Le = shape->get_material()->emitted(0, 0, p_pos);
        p_power = (Le * PI * area) / total_photons;
    }

    virtual bool emit_targeted(
        glm::vec3& p_pos, glm::vec3& p_dir, glm::vec3& p_power, float total_photons, const Object& target
    ) const override {
        glm::vec3 light_normal;
        float area;
        shape->sample_surface(p_pos, light_normal, area);
        if (area <= EPSILON) return false;

        glm::vec3 vec_to_target = target.random_pointing_vector(p_pos);
        float dist = glm::length(vec_to_target);
        
        if (dist <= EPSILON) return false;
        p_dir = vec_to_target / dist;

        float cos_theta = glm::dot(light_normal, p_dir);
        if (cos_theta <= 0.0f) {
            p_power = glm::vec3(0.0f);
            return false;
        }
        float pdf_dir = target.pdf_value(p_pos, p_dir);
        
        if (pdf_dir <= EPSILON) return false;
        glm::vec3 Le = shape->get_material()->emitted(0.0f, 0.0f, p_pos);
        float total_pdf = pdf_dir / area;
        p_power = (Le * cos_theta) / (total_photons * total_pdf);

        return true;
    }

public:
    std::shared_ptr<Object> shape;
};