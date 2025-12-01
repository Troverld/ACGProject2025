cmake_minimum_required(VERSION 3.14)
project(MyPathTracer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- 1. 查找 OpenMP ---
find_package(OpenMP)

# --- 1. 自动下载并配置 GLM (FetchContent) ---
include(FetchContent)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(glm)

# --- 2. 配置包含目录 ---
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/external)

# --- 3. 配置输出路径 (强制所有配置都输出到 bin) ---
set(OUTPUT_DIR ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${OUTPUT_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${OUTPUT_DIR})

# --- 4. 添加源文件 ---
set(SOURCES
    src/main.cpp
)

# --- 5. 生成可执行文件 ---
add_executable(MyPathTracer ${SOURCES})

# --- 6. 链接库与编译选项 ---
target_link_libraries(MyPathTracer PRIVATE glm::glm)

if(OpenMP_CXX_FOUND)
    target_link_libraries(MyPathTracer PRIVATE OpenMP::OpenMP_CXX)
    message(STATUS "OpenMP found and linked.")
else()
    message(WARNING "OpenMP not found. Compiling without parallelization.")
endif()

if(MSVC)
    # 解决中文注释编码警告 (C4819)
    target_compile_options(MyPathTracer PRIVATE /utf-8)
    # 解决 sprintf 不安全警告 (C4996)
    target_compile_definitions(MyPathTracer PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()