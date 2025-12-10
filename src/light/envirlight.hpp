#pragma once

#include "../texture/texture_utils.hpp"
#include "light_utils.hpp"
#include "../core/distribution.hpp"
#include "../texture/image_texture.hpp"
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
    EnvironmentLight(std::shared_ptr<Texture> tex) : texture(tex) {
        // Try to cast to ImageTexture to build Importance Sampling CDF
        auto img_tex = std::dynamic_pointer_cast<ImageTexture>(tex);
        
        if (img_tex) {
            int w = img_tex->get_width();
            int h = img_tex->get_height();
            std::vector<float> luminance(w * h);

            for (int v = 0; v < h; ++v) {
                // Equirectangular mapping distortion correction: sin(theta)
                float vp = (v + 0.5f) / float(h);
                float theta = PI * vp;
                float sin_theta = std::sin(theta);
                
                for (int u = 0; u < w; ++u) {
                    glm::vec3 color = img_tex->get_pixel(u, v);
                    float lum = grayscale(color);
                    luminance[v * w + u] = lum * sin_theta;
                }
            }
            distribution = std::make_unique<Distribution2D>(luminance.data(), w, h);
        }
        // If it's not an ImageTexture (e.g. SolidColor), distribution remains null
        // and we fallback to uniform sampling.

        if(distribution){
            this -> est_power = distribution->p_marginal->func_int * (2.0f * PI * PI);
        }else{
            glm::vec3 center_radiance = tex->value(0.5f, 0.5f, glm::vec3(0.0f));
            
            float intensity = grayscale(center_radiance);
            
            this->est_power = intensity * 4.0f * PI;
        }

        // if(this -> est_power < EPSILON) this -> est_power = EPSILON;
    }

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
        // Case 1: Importance Sampling with Image Map
        if (distribution) {
            float map_pdf;
            glm::vec2 uv = distribution->sample_continuous(glm::vec2(random_float(), random_float()), map_pdf);
            
            if (map_pdf == 0) { pdf = 0; return glm::vec3(0); }

            wi = uv_to_sphere(uv.x, uv.y);

            float sin_theta = std::sin(uv.y * PI); 
            
            // PDF conversion: Area Measure (UV) -> Solid Angle Measure
            // pdf_solid = pdf_uv / (2 * PI^2 * sin(theta))
            if (sin_theta == 0) pdf = 0;
            else pdf = map_pdf / (2.0f * PI * PI * sin_theta);

            distance = Infinity;
            return texture->value(uv.x, uv.y, wi);
        }
        // Case 2: Uniform Sampling (Fallback)
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
     */
    virtual float pdf_value(const glm::vec3& origin, const glm::vec3& wi) const override {
        if (distribution) {
            glm::vec3 dir = glm::normalize(wi);
            float u, v;
            get_sphere_uv(dir, u, v);
            
            float map_pdf = distribution->pdf(glm::vec2(u, v));
            float theta = v * PI;
            float sin_theta = std::sin(theta);
            
            if (sin_theta <= EPSILON) return 0.0f;
            return map_pdf / (2.0f * PI * PI * sin_theta);
        }
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
    std::unique_ptr<Distribution2D> distribution;
};