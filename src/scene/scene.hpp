#pragma once

#include "object.hpp"
#include "light.hpp"
#include <vector>
#include <memory>

/**
 * @brief A container for all objects in the scene.
 * Also implements the Object interface, so a Scene can be treated as a single Hittable.
 */
class Scene : public Object {
public:
    Scene() {}
    Scene(std::shared_ptr<Object> object) { add(object); }

    /**
     * @brief Clear all objects and lights from the scene.
     */
    void clear() { 
        objects.clear(); 
        lights.clear();
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
            lights.push_back(std::make_shared<DiffuseAreaLight>(object));
        }
    }

    /**
     * @brief Intersects a ray with all objects in the scene.
     * Finds the closest intersection.
     */
    virtual bool intersect(const Ray& r, float t_min, float t_max, HitRecord& rec) const override {
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
     * @brief Find the bounding box of the scene
     */
    virtual bool bounding_box(float time0, float time1, AABB& output_box) const override {
        if (objects.empty()) return false;

        AABB temp_box;
        bool first_box = true;

        for (const auto& object : objects) {
            if (!object->bounding_box(time0, time1, temp_box)) return false;
            output_box = first_box ? temp_box : surrounding_box(output_box, temp_box);
            first_box = false;
        }

        return true;
    }

    /**
     * @brief Samples the background color for a ray that missed all geometry.
     * Currently implements a simple vertical gradient (blue-ish sky).
     * Future implementations should support HDRI maps here.
     * 
     * @param r The ray that missed the scene geometry.
     * @return glm::vec3 The radiance/color of the background.
     */
    glm::vec3 sample_background(const Ray& r) const {
        glm::vec3 unit_direction = glm::normalize(r.direction());
        // Map y from [-1, 1] to [0, 1]
        float t = 0.5f * (unit_direction.y + 1.0f);
        // Linear interpolation: (1-t)*White + t*Blue
        return (1.0f - t) * glm::vec3(1.0f, 1.0f, 1.0f) + t * glm::vec3(0.5f, 0.7f, 1.0f);
    }
    // Stub implementation for Scene itself acting as an object
    virtual Material* get_material() const override { return nullptr; }

public:
    std::vector<std::shared_ptr<Object>> objects;
    std::vector<std::shared_ptr<Light>> lights;
};