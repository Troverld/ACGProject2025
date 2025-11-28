#pragma once

#include "object.hpp"
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
     * @brief Clear all objects from the scene.
     */
    void clear() { objects.clear(); }

    /**
     * @brief Add an object to the scene.
     * @param object Shared pointer to the object.
     */
    void add(std::shared_ptr<Object> object) { objects.push_back(object); }

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

public:
    std::vector<std::shared_ptr<Object>> objects;
};