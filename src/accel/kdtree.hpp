#pragma once

#include "../core/photon.hpp"
#include <vector>
#include <algorithm>
#include <iostream>
#include "../core/utils.hpp"

/**
 * @brief A balanced KD-Tree for storing and querying Photons.
 * Essential for the 'Radiance Estimation' step in Photon Mapping.
 */
class PhotonMap {
public:
    PhotonMap() {}

    /**
     * @brief Stores a photon into the list.
     * Note: Not balanced until build() is called.
     */
    void add_photon(const Photon& p) {
        photons.push_back(p);
    }

    /**
     * @brief Builds the balanced KD-Tree structure.
     * Uses std::nth_element to organize the flat array into a tree structure implicitly.
     */
    void build() {
        if (photons.empty()) return;
        balance(0, (int)photons.size() - 1);
        std::cout << "[PhotonMap] Built with " << photons.size() << " photons." << std::endl;
    }

    /**
     * @brief Finds all photons within a certain radius of a point.
     * 
     * @param p The query point.
     * @param radius Search radius.
     * @param results Output vector of pointers to found photons.
     */
    void find_in_radius(const glm::vec3& p, float radius, std::vector<const Photon*>& results) const {
        if (photons.empty()) return;
        float r2 = radius * radius;
        find_recursive(0, (int)photons.size() - 1, p, r2, results);
    }

    size_t size() const { return photons.size(); }

private:
    std::vector<Photon> photons;

    /**
     * @brief Recursively balances the tree segment.
     * Selects a splitting axis and effectively sorts the median to the middle.
     */
    void balance(int start, int end) {
        if (start > end) return;

        // 1. Determine splitting axis based on bounding box of this segment
        // (Simplified: we can also just cycle x->y->z based on recursion depth, 
        // but checking extent is more robust for non-cube distributions)
        glm::vec3 min_p(Infinity), max_p(-Infinity);
        for (int i = start; i <= end; ++i) {
            min_p = glm::min(min_p, photons[i].p);
            max_p = glm::max(max_p, photons[i].p);
        }
        glm::vec3 extents = max_p - min_p;
        
        int axis = 0;
        if (extents.y > extents.x) axis = 1;
        if (extents.z > extents[axis]) axis = 2;

        int mid = (start + end) / 2;

        // 2. Partition the vector so the median element is at 'mid'
        // Elements < median go left, > median go right.
        auto comparator = [axis](const Photon& a, const Photon& b) {
            return a.p[axis] < b.p[axis];
        };

        std::nth_element(photons.begin() + start, photons.begin() + mid, photons.begin() + end + 1, comparator);

        photons[mid].plane = axis; // Store split axis in the node itself

        balance(start, mid - 1);
        balance(mid + 1, end);
    }

    void find_recursive(int start, int end, const glm::vec3& p, float r2, std::vector<const Photon*>& results) const {
        if (start > end) return;

        int mid = (start + end) / 2;
        const Photon& curr = photons[mid];

        // 1. Check current node
        float dist_sq = glm::dot(curr.p - p, curr.p - p);
        if (dist_sq <= r2) {
            results.push_back(&curr);
        }

        // 2. Determine which children to visit
        int axis = curr.plane;
        float diff = p[axis] - curr.p[axis]; // Signed distance to plane

        // If target is left of plane, search left child
        if (diff < 0) {
            find_recursive(start, mid - 1, p, r2, results);
            // If the sphere overlaps the plane, we must ALSO search the right child
            if (diff * diff < r2) {
                find_recursive(mid + 1, end, p, r2, results);
            }
        } else {
            // Target is right of plane, search right child
            find_recursive(mid + 1, end, p, r2, results);
            if (diff * diff < r2) {
                find_recursive(start, mid - 1, p, r2, results);
            }
        }
    }
};