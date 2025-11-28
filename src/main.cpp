#include <iostream>
#include <vector>
#include <string>

// --- External Libs ---
// GLM: 数学库
#include <glm/glm.hpp>
#include <glm/vec3.hpp>

// STB: 图片输出
// 注意：必须在一个 .cpp 文件中定义 STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

int main() {
    // 1. 设置图像参数
    const int width = 800;
    const int height = 600;
    const int channels = 3; // RGB

    std::cout << "Rendering a " << width << "x" << height << " image..." << std::endl;

    // 2. 验证 GLM 是否工作
    glm::vec3 test_vec(1.0f, 2.0f, 3.0f);
    glm::vec3 test_result = test_vec * 2.0f;
    std::cout << "GLM Test: (" << test_result.x << ", " << test_result.y << ", " << test_result.z << ")" << std::endl;
    // 预期输出: (2, 4, 6)

    // 3. 分配像素 buffer (unsigned char: 0-255)
    // 大小 = 宽 * 高 * 通道数
    std::vector<unsigned char> image(width * height * channels);

    // 4. 填充像素 (简单的 UV 渐变)
    for (int j = 0; j < height; ++j) {
        for (int i = 0; i < width; ++i) {
            // 计算像素在数组中的索引
            // 图像原点通常在左上角，但在图形学中有时习惯左下角。
            // 这里我们按照 stbi 的习惯，行优先，从上到下。
            int index = (j * width + i) * channels;

            // 生成颜色 (0.0 - 1.0)
            float r = float(i) / (width - 1);
            float g = float(j) / (height - 1);
            float b = 0.25f;

            // 转换到 0-255
            image[index + 0] = static_cast<unsigned char>(255.99f * r);
            image[index + 1] = static_cast<unsigned char>(255.99f * g);
            image[index + 2] = static_cast<unsigned char>(255.99f * b);
        }
    }

    // 5. 保存图片
    // stbi_write_png(文件名, 宽, 高, 通道数, 数据指针, 跨度(每行的字节数))
    // 跨度 = width * channels
    const std::string filename = "test_output.png";
    int result = stbi_write_png(filename.c_str(), width, height, channels, image.data(), width * channels);

    if (result) {
        std::cout << "Image saved successfully to bin/" << filename << std::endl;
    } else {
        std::cerr << "Failed to save image!" << std::endl;
        return -1;
    }

    return 0;
}