#pragma once

#include "../core/utils.hpp"
#include "../scene/object.hpp"
#include <algorithm>
#include <vector>
#include <memory>
#include <iostream>

class BVHNode : public Object {
public:
    BVHNode() {}

    BVHNode(const std::vector<std::shared_ptr<Object>>& src_objects, float time0, float time1)
        : BVHNode(src_objects, 0, src_objects.size(), time0, time1)
    {}

    BVHNode(const std::vector<std::shared_ptr<Object>>& src_objects, 
            size_t start, size_t end, float time0, float time1) 
    {
        auto objects = src_objects; 

        // 1. 随机选择一个轴进行划分 (更高级的SAH会在这里计算最优轴)
        // 注意：这里我们将轴记录下来，供 intersect 使用
        int axis = random_int(0, 2);
        
        // 比较函数
        auto comparator = (axis == 0) ? box_x_compare
                        : (axis == 1) ? box_y_compare
                        :               box_z_compare;

        size_t object_span = end - start;

        if (object_span == 1) {
            left = right = objects[start];
        } else if (object_span == 2) {
            if (comparator(objects[start], objects[start+1])) {
                left = objects[start];
                right = objects[start+1];
            } else {
                left = objects[start+1];
                right = objects[start];
            }
        } else {
            std::sort(objects.begin() + start, objects.begin() + end, comparator);

            size_t mid = start + object_span / 2;
            
            // 递归构建
            left = std::make_shared<BVHNode>(objects, start, mid, time0, time1);
            right = std::make_shared<BVHNode>(objects, mid, end, time0, time1);
        }

        // 保存本次划分使用的轴
        this->split_axis = axis; 

        AABB box_left, box_right;
        bool has_box_l = left->bounding_box(time0, time1, box_left);
        bool has_box_r = right->bounding_box(time0, time1, box_right);

        if (!has_box_l && !has_box_r) box = AABB();
        else if (has_box_l && !has_box_r) box = box_left;
        else if (!has_box_l && has_box_r) box = box_right;
        else box = surrounding_box(box_left, box_right);
    }

    /**
     * @brief 优化后的遍历：考虑光线方向
     */
    virtual bool intersect(const Ray& r, float t_min, float t_max, HitRecord& rec) const override {
        if (!box.hit(r, t_min, t_max))
            return false;

        // 优化的核心：Front-to-Back Traversal
        // 如果光线在划分轴上的分量为正，则 Left 节点通常在“前面”（坐标较小），Right 在“后面”（坐标较大）。
        // 如果为负，则反之。
        bool visit_left_first = r.direction()[split_axis] >= 0;

        const auto& first = visit_left_first ? left : right;
        const auto& second = visit_left_first ? right : left;

        // 1. 查最近的子节点
        bool hit_first = first->intersect(r, t_min, t_max, rec);
        
        // 2. 查较远的子节点
        // 关键点：如果 first 命中了，我们将 t_max 缩小为 rec.t
        // 这样如果 second 的包围盒比 rec.t 还远，box.hit 就会直接返回 false，从而剪枝。
        bool hit_second = second->intersect(r, t_min, hit_first ? rec.t : t_max, rec);

        return hit_first || hit_second;
    }

    virtual bool bounding_box(float time0, float time1, AABB& output_box) const override {
        output_box = box;
        return true;
    }
    
    virtual Material* get_material() const override { return nullptr; }

public:
    std::shared_ptr<Object> left;
    std::shared_ptr<Object> right;
    AABB box;
    int split_axis = 0; // 新增：记录该节点是按哪个轴划分的

private:
    static bool box_compare(const std::shared_ptr<Object> a, const std::shared_ptr<Object> b, int axis) {
        AABB box_a, box_b;
        if (!a->bounding_box(0, 0, box_a) || !b->bounding_box(0, 0, box_b))
            std::cerr << "No bounding box in BVH constructor.\n";
        return box_a.min_point()[axis] < box_b.min_point()[axis];
    }
    static bool box_x_compare(const std::shared_ptr<Object> a, const std::shared_ptr<Object> b) {
        return box_compare(a, b, 0);
    }
    static bool box_y_compare(const std::shared_ptr<Object> a, const std::shared_ptr<Object> b) {
        return box_compare(a, b, 1);
    }
    static bool box_z_compare(const std::shared_ptr<Object> a, const std::shared_ptr<Object> b) {
        return box_compare(a, b, 2);
    }
};