#pragma once
#include "texture_utils.hpp"
#include "../core/utils.hpp"
#include <vector>
#include <numeric>
#include <cmath>
#include <algorithm>

/**
 * @brief Helper class to generate Perlin Noise (Gradient Noise).
 * Contains the permutation tables and random vectors required for noise generation.
 */
class PerlinNoise {
public:
    PerlinNoise() {
        // 1. Initialize random unit vectors (gradients)
        ranvec = std::vector<glm::vec3>(point_count);
        for (int i = 0; i < point_count; ++i) {
            ranvec[i] = glm::normalize(random_vec3(-1.0f, 1.0f));
        }

        // 2. Initialize permutation tables
        perm_x = perlin_generate_perm();
        perm_y = perlin_generate_perm();
        perm_z = perlin_generate_perm();
    }

    /**
     * @brief Samples the single-frequency noise at a given point.
     * Uses trilinear interpolation with Hermitian smoothing.
     * 
     * @param p The 3D point to sample.
     * @return float Noise value (usually between -1 and 1).
     */
    float noise(const glm::vec3& p) const {
        float u = p.x - floor(p.x);
        float v = p.y - floor(p.y);
        float w = p.z - floor(p.z);

        // Grid cell coordinates
        int i = static_cast<int>(floor(p.x));
        int j = static_cast<int>(floor(p.y));
        int k = static_cast<int>(floor(p.z));

        glm::vec3 c[2][2][2];

        // Fetch gradients for the cube corners
        for (int di = 0; di < 2; di++)
            for (int dj = 0; dj < 2; dj++)
                for (int dk = 0; dk < 2; dk++)
                    c[di][dj][dk] = ranvec[
                        perm_x[(i + di) & 255] ^
                        perm_y[(j + dj) & 255] ^
                        perm_z[(k + dk) & 255]
                    ];

        return perlin_interp(c, u, v, w);
    }

    /**
     * @brief Generates turbulence by summing multiple frequencies of noise.
     * Gives a fractal appearance suitable for marble or clouds.
     * 
     * @param p Point to sample.
     * @param depth Number of layers (octaves).
     * @return float Turbulence value (always positive).
     */
    float turb(const glm::vec3& p, int depth = 7) const {
        float accum = 0.0f;
        glm::vec3 temp_p = p;
        float weight = 1.0f;

        for (int i = 0; i < depth; i++) {
            accum += weight * noise(temp_p);
            weight *= 0.5f;
            temp_p *= 2.0f;
        }

        return std::abs(accum);
    }

private:
    static const int point_count = 256;
    std::vector<glm::vec3> ranvec;
    std::vector<int> perm_x;
    std::vector<int> perm_y;
    std::vector<int> perm_z;

    static std::vector<int> perlin_generate_perm() {
        std::vector<int> p(point_count);
        std::iota(p.begin(), p.end(), 0); // Fill with 0, 1, ..., 255
        permute(p, point_count);
        return p;
    }

    static void permute(std::vector<int>& p, int n) {
        for (int i = n - 1; i > 0; i--) {
            int target = random_int(0, i);
            std::swap(p[i], p[target]);
        }
    }

    /**
     * @brief Trilinear interpolation with Hermite Cubic Smoothing.
     */
    static float perlin_interp(glm::vec3 c[2][2][2], float u, float v, float w) {
        // Hermite smoothing: 3t^2 - 2t^3
        float uu = u * u * (3 - 2 * u);
        float vv = v * v * (3 - 2 * v);
        float ww = w * w * (3 - 2 * w);
        
        float accum = 0.0f;
        for (int i = 0; i < 2; i++)
            for (int j = 0; j < 2; j++)
                for (int k = 0; k < 2; k++) {
                    // Vector from corner to point
                    glm::vec3 weight_v(u - i, v - j, w - k);
                    
                    accum += (i * uu + (1 - i) * (1 - uu)) *
                             (j * vv + (1 - j) * (1 - vv)) *
                             (k * ww + (1 - k) * (1 - ww)) *
                             glm::dot(c[i][j][k], weight_v);
                }
        return accum;
    }
};

/**
 * @brief Texture generated using Perlin Noise.
 * Implements a marble-like pattern using turbulence.
 */
class Perlin : public Texture {
public:
    Perlin() : scale(1.0f) {}
    Perlin(float sc) : scale(sc) {}

    /**
     * @brief Samples the Perlin texture.
     * 
     * @param p Point in 3D space.
     * @return glm::vec3 Color value.
     */
    virtual glm::vec3 value(float u, float v, const glm::vec3& p) const override {
        // Marble logic: sin(scale * z + 10 * turb(p))
        // This creates wavy strips along Z, perturbed by noise.
        // Mapped to [0, 1] range.
        return glm::vec3(1.0f) * 0.5f * (1.0f + sin(scale * p.z + 10.0f * noise.turb(p)));
    }

public:
    PerlinNoise noise;
    float scale;
};