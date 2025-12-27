

# 1. 手动白名单 (MANUAL WHITELIST)
# 在这里填入你想要显式包含的文件名（带后缀）。
# 脚本会自动在项目中匹配这些文件名。
MANUAL_WHITELIST = {
    # "arealight.hpp",
    # "envirlight.hpp",
    # "pointlight.hpp",
    "mesh.hpp",
    # "moving_sphere.hpp",
    # "sphere.hpp",
    "triangle.hpp",
    # "volume.hpp",
    # "scene.hpp",
    # "diffuse.hpp",
    # "emitter.hpp",
    # "glass.hpp",
    # "metal.hpp",
    # "phase_function.hpp",
    # "camera.hpp",
    "image_texture.hpp",
    # "integrator_utils.hpp",
    # "photon_integrator.hpp",
    # "dispersive.hpp",
    # "main.cpp",
    # "scene_list.cpp",
    # "path_integrator.hpp",
}


import os

# ==========================================
#              配置区域 (Configuration)
# ==========================================

# 2. 输出文件名
OUTPUT_FILENAME = "project_code_context.txt"
# 3. 需要强制忽略的目录 (GLOBAL IGNORE)
# 这些目录完全不会被扫描，优先级最高
IGNORE_DIRS = {
    'build', 'bin', '.git', '.vscode', '.idea', 
    'external', 'assets', '__pycache__',
    'cmake-build-debug', 'cmake-build-release'
}

# 4. 需要忽略的具体文件
IGNORE_FILES = {
    os.path.basename(__file__), # 忽略脚本自己
    OUTPUT_FILENAME,
    '.DS_Store'
}

# 5. 允许的文件后缀 (防止误读二进制文件)
ALLOWED_EXTENSIONS = {'.cpp', '.hpp', '.h', '.c', '.cc', '.txt', '.cmake'}

# ==========================================
#              逻辑实现 (Logic)
# ==========================================

def is_text_file(filename):
    """基础检查：文件是否为文本/代码格式"""
    _, ext = os.path.splitext(filename)
    return ext.lower() in ALLOWED_EXTENSIONS or filename == "CMakeLists.txt"

def is_auto_whitelisted(rel_path_unix, filename):
    """
    检查是否满足【自动白名单】规则：
    1. src/core/ 下的所有文件
    2. [light|object|texture|material]_utils.hpp
    3. arch.txt
    """
    
    # 规则 1: src/core/ 下的所有文件
    if rel_path_unix.startswith("src/core/"):
        return True

    # 规则 2: 四大基类 Utils 头文件
    # 定义需要自动包含的基类文件名集合
    base_utils = {
        "light_utils.hpp",
        "object_utils.hpp",
        "texture_utils.hpp",
        "material_utils.hpp"
    }
    if filename in base_utils:
        return True
    
    # 规则 3: 架构说明文件
    if filename == "arch.txt":
        return True

    return False

def should_pack(file_path, project_root):
    """核心判断逻辑：是否应该打包该文件"""
    filename = os.path.basename(file_path)
    
    # 0. 基础过滤：如果是忽略文件或非文本文件，直接跳过
    if filename in IGNORE_FILES or not is_text_file(filename):
        return False
        
    # 获取相对路径，并统一转换为 Unix 风格 (使用 /) 方便匹配
    rel_path = os.path.relpath(file_path, project_root)
    rel_path_unix = rel_path.replace(os.sep, '/')

    # 1. 检查【自动白名单】
    if is_auto_whitelisted(rel_path_unix, filename):
        return True

    # 2. 检查【手动白名单】
    # 只要文件名在清单里，就包含（不用写全路径，方便）
    if filename in MANUAL_WHITELIST:
        return True
    
    # 如果你也想支持写相对路径（如 "src/main.cpp"），可以取消下面这行的注释：
    # if rel_path_unix in MANUAL_WHITELIST: return True

    return False

def pack_project():
    project_root = os.getcwd()
    output_path = os.path.join(project_root, OUTPUT_FILENAME)
    
    count = 0
    
    try:
        with open(output_path, 'w', encoding='utf-8') as outfile:
            # 遍历目录
            for root, dirs, files in os.walk(project_root):
                # 优化：原地修改 dirs 以移除忽略目录，阻止进入扫描
                dirs[:] = [d for d in dirs if d not in IGNORE_DIRS]
                
                for file in files:
                    file_path = os.path.join(root, file)
                    
                    if should_pack(file_path, project_root):
                        rel_path = os.path.relpath(file_path, project_root)
                        print(f"Packing: {rel_path}")
                        
                        try:
                            with open(file_path, 'r', encoding='utf-8') as infile:
                                content = infile.read()
                                
                                # 写入格式化内容
                                outfile.write("=" * 80 + "\n")
                                outfile.write(f"File: {rel_path}\n")
                                outfile.write("=" * 80 + "\n\n")
                                outfile.write("```cpp\n" + content + "\n```\n\n")
                                count += 1
                                
                        except UnicodeDecodeError:
                            print(f"Warning: Could not decode {rel_path}, skipping.")
                        except Exception as e:
                            print(f"Error reading {rel_path}: {e}")
                            
    except IOError as e:
        print(f"Error opening output file: {e}")

    print("-" * 40)
    print(f"Done! Packed {count} files into: {OUTPUT_FILENAME}")

if __name__ == "__main__":
    pack_project()