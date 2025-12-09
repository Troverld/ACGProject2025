#pragma once

#include "../core/utils.hpp"
#include "../object/object_utils.hpp"
#include <algorithm>
#include <vector>
#include <memory>
#include <iostream>

class BVHNode : public Object {
public:
    BVHNode() {}

    /**
     * @brief Public constructor that takes a list of objects.
     * It creates a copy of the vector once, then passes it by reference to the recursive constructor.
     */
    BVHNode(const std::vector<std::shared_ptr<Object>>& src_objects, float time0, float time1)
        : BVHNode(src_objects, 0, src_objects.size(), time0, time1)
    {}

    /**
     * @brief Recursive constructor.
     * NOTE: We pass 'objects' by reference (non-const) to allow sorting in place without copying.
     */
    BVHNode(std::vector<std::shared_ptr<Object>>& objects, 
            size_t start, size_t end, float time0, float time1) 
    {
        // 1. Choose a random axis (0=x, 1=y, 2=z) to split the objects
        // (Advanced BVH uses SAH, but random axis is acceptable for simple implementations)
        int axis = random_int(0, 2);
        this->split_axis = axis; // Store axis for optimized intersection

        // 2. Define the comparator lambda function
        // We must use the time interval (time0, time1) to get the correct bounding box for moving objects.
        auto comparator = [=](const std::shared_ptr<Object>& a, const std::shared_ptr<Object>& b) {
            AABB box_a, box_b;
            if (!a->bounding_box(time0, time1, box_a) || !b->bounding_box(time0, time1, box_b))
                std::cerr << "No bounding box in BVHNode constructor.\n";

            return box_a.min_point()[axis] < box_b.min_point()[axis];
        };

        size_t object_span = end - start;

        if (object_span == 1) {
            // Leaf node with 1 object: duplicate the pointer to avoid null checks later
            left = right = objects[start];
        } else if (object_span == 2) {
            // Leaf node with 2 objects: just sort them directly
            if (comparator(objects[start], objects[start+1])) {
                left = objects[start];
                right = objects[start+1];
            } else {
                left = objects[start+1];
                right = objects[start];
            }
        } else {
            // Internal node: Sort the range and split in half
            std::sort(objects.begin() + start, objects.begin() + end, comparator);

            size_t mid = start + object_span / 2;
            
            // Recursively build children
            // Note: We pass the SAME 'objects' vector reference
            left = std::make_shared<BVHNode>(objects, start, mid, time0, time1);
            right = std::make_shared<BVHNode>(objects, mid, end, time0, time1);
        }

        // 3. Compute the bounding box for this node
        AABB box_left, box_right;
        bool has_box_l = left->bounding_box(time0, time1, box_left);
        bool has_box_r = right->bounding_box(time0, time1, box_right);

        if (!has_box_l && !has_box_r) box = AABB();
        else if (has_box_l && !has_box_r) box = box_left;
        else if (!has_box_l && has_box_r) box = box_right;
        else box = surrounding_box(box_left, box_right);
    }

    /**
     * @brief Optimized intersection test using Front-to-Back Traversal.
     */
    virtual bool intersect(const Ray& r, float t_min, float t_max, HitRecord& rec) const override {
        // 1. Check if the ray hits the bounding box of this node
        if (!box.hit(r, t_min, t_max))
            return false;

        // 2. Determine traversal order based on ray direction.
        // If the ray direction is positive along the split axis, the 'left' child (smaller coordinates)
        // is likely closer. If negative, the 'right' child is likely closer.
        bool visit_left_first = r.direction()[split_axis] >= 0;

        const auto& first = visit_left_first ? left : right;
        const auto& second = visit_left_first ? right : left;

        // 3. Check the closer child (first)
        bool hit_first = first->intersect(r, t_min, t_max, rec);
        
        // 4. Check the farther child (second)
        // Optimization: If 'first' hit something, we shorten t_max to 'rec.t'.
        // This allows the bounding box check of 'second' to fail early if it is farther away than the hit in 'first'.
        bool hit_second = second->intersect(r, t_min, hit_first ? rec.t : t_max, rec);

        return hit_first || hit_second;
    }

    virtual bool bounding_box(float time0, float time1, AABB& output_box) const override {
        output_box = box;
        return true;
    }
    
    // BVH nodes themselves do not have materials; only the leaf primitives do.
    virtual Material* get_material() const override { return nullptr; }
    virtual float pdf_value(const glm::vec3& origin, const glm::vec3& v) const override { return 0.0f; }
    virtual glm::vec3 random_pointing_vector(const glm::vec3& origin) const override { return glm::vec3(0.0f); }
    virtual void sample_surface(glm::vec3& pos, glm::vec3& normal, float& area) const override {}

public:
    std::shared_ptr<Object> left;
    std::shared_ptr<Object> right;
    AABB box;
    int split_axis = 0; // The axis (0, 1, or 2) used to split this node
    
private:
    // Helper constructor to allow the public one to copy the vector once
    BVHNode(const std::vector<std::shared_ptr<Object>>& src_objects, 
            size_t start, size_t end, float time0, float time1)
    {
        // This constructor delegates to the reference-based constructor
        auto objects = src_objects; // Copy happens HERE only once for the root
        *this = BVHNode(objects, start, end, time0, time1);
    }
};