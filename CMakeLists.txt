cmake_minimum_required(VERSION 3.14)
project(MyPathTracer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- 1. 自动下载并配置 GLM (FetchContent) ---
include(FetchContent)
message(STATUS "Fetching GLM...")
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        0.9.9.8 # 指定稳定版本
)
FetchContent_MakeAvailable(glm)

# --- 2. 配置包含目录 ---
# 包含 src 目录，方便内部 include
include_directories(${CMAKE_SOURCE_DIR}/src)
# 包含 external 目录，方便引用 stb_image_write.h
include_directories(${CMAKE_SOURCE_DIR}/external)

# --- 3. 添加源文件 ---
# 这一步目前只有 main.cpp，后续加文件可以在这里追加，或者使用 file(GLOB...)
set(SOURCES
    src/main.cpp
)

# --- 4. 生成可执行文件 ---
add_executable(MyPathTracer ${SOURCES})

# --- 5. 链接库 ---
# 链接 GLM
target_link_libraries(MyPathTracer PRIVATE glm::glm)

# 设置输出目录为 bin 文件夹，防止污染根目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)