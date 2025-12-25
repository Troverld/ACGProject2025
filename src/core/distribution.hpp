#pragma once
#include <vector>
#include <algorithm>
#include <numeric>
#include "utils.hpp"

/**
 * @brief Handles 1D Probability Distribution Function (PDF) and Cumulative Distribution Function (CDF).
 * Used for importance sampling arrays of data (e.g., texture rows, light lists).
 */
struct Distribution1D {
    std::vector<float> func; // The function values (e.g., luminance)
    std::vector<float> cdf;  // The cumulative distribution
    float func_int;          // The integral of the function

    Distribution1D(const float* f, int n) : func(f, f + n), cdf(n + 1) {
        cdf[0] = 0;
        for (int i = 0; i < n; ++i) {
            cdf[i + 1] = cdf[i] + func[i] / n;
        }
        func_int = cdf[n];
        if (func_int == 0) {
            for (int i = 1; i < n + 1; ++i) cdf[i] = float(i) / float(n);
        } else {
            for (int i = 1; i < n + 1; ++i) cdf[i] /= func_int;
        }
    }

    Distribution1D(const float* f, size_t n) : Distribution1D(f, static_cast<int>(n)) {}

    int count() const { return static_cast<int>(func.size()); }

    /**
     * @brief Sample the distribution.
     * @param u Random number [0, 1].
     * @param pdf [out] Probability density of the sampled value.
     * @param off [out] Integer offset (index).
     * @return float Continuous offset.
     */
    float sample_continuous(float u, float& pdf, int& off) const {
        // Find surrounding CDF segments using binary search
        auto ptr = std::upper_bound(cdf.begin(), cdf.end(), u);
        int offset = std::max(0, int(ptr - cdf.begin() - 1));
        off = offset;
        
        // Compute offset along the segment
        float du = u - cdf[offset];
        if ((cdf[offset + 1] - cdf[offset]) > 0) {
            du /= (cdf[offset + 1] - cdf[offset]);
        }
        pdf = func[offset] / (func_int > 0 ? func_int : 1.0f);
        return (offset + du) / count();
    }
    
    /**
     * @brief Sample discrete index.
     */
    int sample_discrete(float u, float& pdf, float& remapped_u) const {
        auto ptr = std::upper_bound(cdf.begin(), cdf.end(), u);
        int offset = std::max(0, int(ptr - cdf.begin() - 1));
        pdf = (func_int > 0) ? func[offset] / (func_int * count()) : 1.0f / count();
        
        // Remap u to [0,1) for the next sampling dimension
        remapped_u = (u - cdf[offset]) / (cdf[offset+1] - cdf[offset]);
        return offset;
    }
    
    // Discrete probability of a specific index
    float pdf_discrete(int index) const {
        if (index < 0 || index >= count()) return 0.0f;
        return (func_int > 0) ? func[index] / (func_int * count()) : 1.0f / count();
    }
};

/**
 * @brief Handles 2D Distribution (e.g., for Image Textures).
 * Composed of one marginal distribution (y-axis) and N conditional distributions (x-axis).
 */
class Distribution2D {
public:
    std::vector<std::unique_ptr<Distribution1D>> p_conditional_v;
    std::unique_ptr<Distribution1D> p_marginal;

    Distribution2D(const float* data, int nu, int nv) {
        for (int v = 0; v < nv; ++v) {
            p_conditional_v.emplace_back(std::make_unique<Distribution1D>(&data[v * nu], nu));
        }
        
        std::vector<float> marginal_func;
        for (int v = 0; v < nv; ++v) {
            marginal_func.push_back(p_conditional_v[v]->func_int);
        }
        p_marginal = std::make_unique<Distribution1D>(marginal_func.data(), nv);
    }

    /**
     * @brief Sample (u, v) from the 2D distribution.
     * @param u Random numbers (u0, u1).
     * @param pdf [out] The joint PDF p(u,v).
     */
    glm::vec2 sample_continuous(const glm::vec2& u, float& pdf) const {
        float pdfs[2];
        int v_idx;
        float d1 = p_marginal->sample_continuous(u.y, pdfs[1], v_idx);
        
        int u_idx;
        float d0 = p_conditional_v[v_idx]->sample_continuous(u.x, pdfs[0], u_idx);
        
        pdf = pdfs[0] * pdfs[1];
        return glm::vec2(d0, d1);
    }

    float pdf(const glm::vec2& p) const {
        int iu = std::clamp(int(p.x * p_conditional_v[0]->count()), 0, p_conditional_v[0]->count() - 1);
        int iv = std::clamp(int(p.y * p_marginal->count()), 0, p_marginal->count() - 1);
        
        return p_conditional_v[iv]->func[iu] / p_marginal->func_int;
    }
};