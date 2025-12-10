#pragma once

#include "../object/object_utils.hpp"
#include "../texture/texture_utils.hpp"
#include "../light/light_agg.hpp"
#include <vector>
#include <memory>
#include "../accel/BVH.hpp"
/**
 * @brief A container for all objects in the scene.
 * ~~Also implements the Object interface, so a Scene can be treated as a single Hittable.~~
 */
class Scene{
public:
    Scene() {}
    // Scene(std::shared_ptr<Object> object) { add(object); }
    /**
     * @brief Set the background texture and register it as an Environment Light.
     * This enables Next Event Estimation (NEE) for the background.
     */
    void set_background(std::shared_ptr<Texture> bg) {
        env_light = std::make_shared<EnvironmentLight>(bg);
    }
    /**
     * @brief Clear all objects and lights from the scene.
     */
    void clear() { 
        objects.clear(); 
        lights.clear();
        bvh_root = nullptr;
    }

    /**
     * @brief The Magic of Automatic Promotion.
     * When an object is added, we check its material. 
     * If it glows, we create a Light entity for it.
     */
    void add(std::shared_ptr<Object> object) {
        objects.push_back(object);
        
        // Check if the object has a material and if it is emissive
        if (object->get_material() && object->get_material()->is_emissive()) {
            auto area_light = std::make_shared<DiffuseAreaLight>(object);
            if (area_light->power() > EPSILON) {
                object->set_light_id(static_cast<int>(lights.size()));
                lights.push_back(area_light);
            }
        }
        bvh_root = nullptr; 
    }

    /**
     * @brief Manually register a light source.
     * Use this for non-geometric lights like PointLight or DirectionalLight.
     */
    void add_light(std::shared_ptr<Light> light) {
        if (light->power() > EPSILON) {
            lights.push_back(light);
        }
    }

    /**
     * @brief Construct the BVH structure.
     * MUST be called before rendering starts for acceleration to take effect.
     * 
     * @param t0 Start time for motion blur.
     * @param t1 End time for motion blur.
     */
    void build_bvh(float t0 = 0.0f, float t1 = 1.0f) {
        if (objects.empty()) return;
        
        std::cout << "Building BVH for " << objects.size() << " objects..." << std::endl;
        bvh_root = std::make_shared<BVHNode>(objects, t0, t1);
    }

    /**
     * @brief Intersects a ray with all objects in the scene.
     * Finds the closest intersection.
     */
    bool intersect(const Ray& r, float t_min, float t_max, HitRecord& rec) const {
        // Use BVH if available
        if (bvh_root) {
            return bvh_root->intersect(r, t_min, t_max, rec);
        }

        HitRecord temp_rec;
        bool hit_anything = false;
        float closest_so_far = t_max;

        for (const auto& object : objects) {
            if (object->intersect(r, t_min, closest_so_far, temp_rec)) {
                hit_anything = true;
                closest_so_far = temp_rec.t; // Update closest t
                rec = temp_rec;              // Update output record
            }
        }

        return hit_anything;
    }

        /**
     * @brief Calculates the transmittance (visibility) between a ray's origin and a maximum distance.
     * Traces shadow rays. If an opaque object is hit, returns black (0).
     * If a transparent (specular) object is hit, it continues tracing but attenuates the light.
     * 
     * @param r The shadow ray.
     * @param max_distance The distance to the light source.
     * @return glm::vec3 The fraction of light that gets through [0, 1].
     */
    glm::vec3 transmittance(const Ray& r, float max_distance, int max_bounce) const {
        glm::vec3 throughput(1.0f);
        Ray current_ray = r;
        float remaining_dist = max_distance;
        
        for(int _=0; _ < max_bounce; ++_) {
            HitRecord rec;
            // Check intersection up to the remaining distance to the light
            if (!intersect(current_ray, SHADOW_EPSILON, remaining_dist, rec)) return throughput;
            // We hit something before the light.
                
            // If it's a transparent material (like glass), let light pass through.
            if (rec.mat_ptr->is_transparent()) {
                // Simple Beer's law approximation or Fresnel loss.
                // Assume 90% throughput per surface for simplicity in this model.
                throughput *= rec.mat_ptr->evaluate_transmission(rec);
                if(near_zero(throughput)) return glm::vec3(0.0f);
                
                // Move the ray forward past the object
                current_ray = Ray(rec.p, current_ray.direction(), current_ray.time());
                remaining_dist -= rec.t;
            } else {
                // Hit an opaque object (occluder). Shadow is black.
                return glm::vec3(0.0f);
            }
        }

        // Exceeded max shadow depth, assume blocked or negligible light
        return glm::vec3(0.0f);
    }

    /**
     * @brief Samples the background color for a ray that missed all geometry.
     * Supports HDRI maps or default gradient.
     * 
     * @param r The ray that missed the scene geometry.
     * @return glm::vec3 The radiance/color of the background.
     */
    glm::vec3 sample_background(const Ray& r) const {
        // --- ADDED: Texture Sampling ---
        if (env_light) return env_light->eval(r.direction());

        return glm::vec3(0.0f, 0.0f, 0.0f);
        // // Default Gradient Fallback
        // glm::vec3 unit_direction = glm::normalize(r.direction());
        // // Map y from [-1, 1] to [0, 1]
        // float t = 0.5f * (unit_direction.y + 1.0f);
        // // Linear interpolation: (1-t)*White + t*Blue
        // return (1.0f - t) * glm::vec3(1.0f, 1.0f, 1.0f) + t * glm::vec3(0.5f, 0.7f, 1.0f);
    }
    // Stub implementation for Scene itself acting as an object
    // virtual Material* get_material() const override { return nullptr; }

public:
    std::vector<std::shared_ptr<Object>> objects;
    std::vector<std::shared_ptr<Light>> lights;  // Area lights only
    std::shared_ptr<EnvironmentLight> env_light;           // Dedicated Environment Light (can be nullptr)
    std::shared_ptr<Object> bvh_root; 
};