#pragma once

#include "material_utils.hpp"
#include "../texture/solid_color.hpp"
/**
 * @brief A material that emits light.
 * It does NOT scatter rays (it stops the bounce).
 */
class DiffuseLight : public Material {
public:
    DiffuseLight(const glm::vec3& c) : emit_texture(std::make_shared<SolidColor>(c)) {}
    DiffuseLight(std::shared_ptr<Texture> a) : emit_texture(a) {}

    virtual bool scatter(const Ray& r_in, const HitRecord& rec, ScatterRecord& srec)const override {
        return false; // No scattering, just emission (for now)
    }

    virtual glm::vec3 emitted(float u, float v, const glm::vec3& p) const override {
        return emit_texture->value(u, v, p);
    }
    
    virtual bool is_emissive() const override { return true; }
    
    virtual bool is_specular() const override { return false; }

public:
    std::shared_ptr<Texture> emit_texture;
};