#pragma once

#include "object_utils.hpp"
#include "../material/isotropic_phase.hpp"
#include "../texture/texture_utils.hpp"
#include <iostream>

/**
 * @brief Represents a volume of constant density (Homogeneous Medium).
 * Examples: Thin fog, colored water, smoke in a box.
 * 
 * Logic:
 * The ray hits the boundary object at t1 and t2.
 * The distance inside is d = t2 - t1.
 * We sample a random distance 'hit_distance' based on the density.
 * If hit_distance < d, the ray scattered INSIDE the volume.
 */
class ConstantMedium : public Object {
public:
    /**
     * @brief Construct a new Constant Medium.
     * 
     * @param b The boundary object (e.g., a Sphere or Box).
     * @param d The density of the fog/smoke. Higher = thicker.
     * @param a The texture/color of the medium.
     */
    ConstantMedium(std::shared_ptr<Object> b, float d, std::shared_ptr<Texture> a)
        : boundary(b), neg_inv_density(-1.0f / d), phase_function(std::make_shared<Isotropic>(a)) {}

    /**
     * @brief Overload using color directly.
     */
    ConstantMedium(std::shared_ptr<Object> b, float d, glm::vec3 c)
        : boundary(b), neg_inv_density(-1.0f / d), phase_function(std::make_shared<Isotropic>(c)) {}

    /**
     * @brief Construct a Glowing Volume.
     * @param emit_color The color/intensity of the light emitted by the smoke.
     */
    ConstantMedium(std::shared_ptr<Object> b, float d, glm::vec3 color, glm::vec3 emit_color)
        : boundary(b), neg_inv_density(-1.0f / d), 
          phase_function(std::make_shared<Isotropic>(color, emit_color)) {}
          
    /**
     * @brief Construct a Glowing Volume (Texture based).
     */
    ConstantMedium(std::shared_ptr<Object> b, float d, std::shared_ptr<Texture> a, std::shared_ptr<Texture> emit_tex)
        : boundary(b), neg_inv_density(-1.0f / d), 
          phase_function(std::make_shared<Isotropic>(a, emit_tex)) {}


    virtual bool intersect(const Ray& r, float t_min, float t_max, HitRecord& rec) const override {
        // Print occasional debug info locally if needed, but keep clean for perf
        
        HitRecord rec1, rec2;

        // 1. Find the entry point (allow t_min to be -Infinity to catch containment)
        if (!boundary->intersect(r, -Infinity, Infinity, rec1))
            return false;

        // 2. Find the exit point (must be after entry)
        if (!boundary->intersect(r, rec1.t + SHADOW_EPSILON, Infinity, rec2))
            return false;

        // 3. Process edge cases (clamping)
        if (rec1.t < t_min) rec1.t = t_min;
        if (rec2.t > t_max) rec2.t = t_max;

        if (rec1.t >= rec2.t)
            return false;

        if (rec1.t < 0)
            rec1.t = 0;

        // 4. Calculate distance through the medium
        const float ray_length = glm::length(r.direction());
        const float distance_inside_boundary = (rec2.t - rec1.t) * ray_length;

        // 5. Sample a random distance based on density
        // hit_distance = - (1/density) * log(random)
        const float hit_distance = neg_inv_density * log(random_float());

        // 6. Determine if the ray scattered inside
        if (hit_distance > distance_inside_boundary)
            return false; // Ray passed through the smoke without hitting a particle

        // 7. Record the hit details
        rec.t = rec1.t + hit_distance / ray_length;
        rec.p = r.at(rec.t);

        // Volumetric scattering is isotropic, normal is arbitrary.
        // We set it to (1,0,0) and front_face to true essentially ignoring it in Isotropic::scatter
        rec.normal = glm::vec3(1, 0, 0); 
        rec.front_face = true; 
        
        rec.mat_ptr = phase_function.get();
        rec.object = this;

        return true;
    }

    virtual bool bounding_box(float time0, float time1, AABB& output_box) const override {
        return boundary->bounding_box(time0, time1, output_box);
    }
    
    // Volumes are not typically treated as area lights in this simple model
    virtual float pdf_value(const glm::vec3& origin, const glm::vec3& v) const override {
        return boundary->pdf_value(origin, v);
    }

    virtual glm::vec3 random_pointing_vector(const glm::vec3& origin) const override {
        return boundary->random_pointing_vector(origin);
    }
    
    virtual void sample_surface(glm::vec3& pos, glm::vec3& normal, float& area) const override {
        boundary->sample_surface(pos, normal, area);
    }

    virtual Material* get_material() const override { return phase_function.get(); }

public:
    std::shared_ptr<Object> boundary;
    std::shared_ptr<Material> phase_function;
    float neg_inv_density;
};