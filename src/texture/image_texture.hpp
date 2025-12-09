#pragma once

#include "texture_utils.hpp"
#include "../core/utils.hpp"
#include <iostream>
#include <algorithm> // for std::clamp

#include "stb_image.h" 

/**
 * @brief Texture backed by an image file.
 * Supports both LDR (Standard images) and HDR (Radiance RGBE) formats.
 * Uses Bilinear Interpolation for smooth sampling.
 */
class ImageTexture : public Texture {
public:
    const static int BYTES_PER_PIXEL = 3;

    /**
     * @brief Construct a new Image Texture.
     * 
     * @param filename Path to the image file.
     */
    ImageTexture(const char* filename) {
        int components_per_pixel = BYTES_PER_PIXEL;
        
        // Attempt to load as floating point first (for HDR)
        if (stbi_is_hdr(filename)) {
            data_f = stbi_loadf(filename, &width, &height, &components_per_pixel, components_per_pixel);
            is_hdr = true;
        } else {
            data_u8 = stbi_load(filename, &width, &height, &components_per_pixel, components_per_pixel);
            is_hdr = false;
        }

        if (!data_u8 && !data_f) {
            std::cerr << "ERROR: Could not load texture image file '" << filename << "'.\n";
            width = height = 0;
        }
        
        bytes_per_scanline = BYTES_PER_PIXEL * width;
    }

    ~ImageTexture() {
        if (data_u8) stbi_image_free(data_u8);
        if (data_f)  stbi_image_free(data_f);
    }

    virtual glm::vec3 value(float u, float v, const glm::vec3& p) const override {
        // If no texture data, return solid magenta (debug color)
        if (data_u8 == nullptr && data_f == nullptr)
            return glm::vec3(1, 0, 1);

        // Clamp input texture coordinates to [0,1] x [1,0]
        u = std::clamp(u, 0.0f, 1.0f);
        v = 1.0f - std::clamp(v, 0.0f, 1.0f); // Flip V to image coordinates

        // Map UV to image coordinates
        float i = u * width;
        float j = v * height;

        // --- Bilinear Interpolation ---
        // Get the coordinates of the top-left pixel
        int x0 = static_cast<int>(i - 0.5f);
        int y0 = static_cast<int>(j - 0.5f);
        
        // Local coordinates within the pixel [0, 1]
        float s = i - 0.5f - x0;
        float t = j - 0.5f - y0;

        // Sample 4 neighbors
        glm::vec3 c00 = get_pixel(x0, y0);
        glm::vec3 c10 = get_pixel(x0 + 1, y0);
        glm::vec3 c01 = get_pixel(x0, y0 + 1);
        glm::vec3 c11 = get_pixel(x0 + 1, y0 + 1);

        // Interpolate X
        glm::vec3 c0 = glm::mix(c00, c10, s);
        glm::vec3 c1 = glm::mix(c01, c11, s);

        // Interpolate Y
        return glm::mix(c0, c1, t);
    }

    int get_width() const { return width; } // Added for Importance Sampling Support
    int get_height() const { return height; } // Added for Importance Sampling Support
    /**
     * @brief Helper to fetch pixel color safely (handles wrapping/clamping).
     */
    glm::vec3 get_pixel(int x, int y) const {
        x = std::clamp(x, 0, width - 1);
        y = std::clamp(y, 0, height - 1);

        if (is_hdr && data_f) {
            int index = (y * width + x) * BYTES_PER_PIXEL;
            return glm::vec3(data_f[index], data_f[index+1], data_f[index+2]);
        } else if (data_u8) {
            int index = (y * width + x) * BYTES_PER_PIXEL;
            constexpr float color_scale = 1.0f / 255.0f;
            return glm::vec3(
                data_u8[index] * color_scale,
                data_u8[index+1] * color_scale,
                data_u8[index+2] * color_scale
            );
        }
        return glm::vec3(0.0f);
    }

private:
    unsigned char* data_u8 = nullptr;
    float* data_f = nullptr;
    int width, height;
    int bytes_per_scanline;
    bool is_hdr = false;
};