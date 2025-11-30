import os

# --- 配置区域 ---

# 最终生成的汇总文件名
OUTPUT_FILENAME = "project_code_context.txt"

# 需要抓取的文件后缀
ALLOWED_EXTENSIONS = {'.cpp', '.hpp', '.h', '.c', '.cc', '.txt', '.cmake'}

# 指定需要抓取的具体文件名（包含后缀，用于抓取无后缀文件或特定文件如 CMakeLists.txt）
# 脚本逻辑里包含了 .txt 和 .cmake，所以 CMakeLists.txt 会被自动包含，这里作额外补充
ALLOWED_FILENAMES = {'CMakeLists.txt'}

# 需要完全忽略的目录名
IGNORE_DIRS = {
    'build', 
    'bin', 
    '.git', 
    '.vscode', 
    '.idea', 
    'external',  # 通常不把 stb_image 或 glm 的源码发给 AI，太长了
    '__pycache__',
    'cmake-build-debug',
    'cmake-build-release'
}

# 需要忽略的具体文件名（比如脚本自己，和生成的输出文件）
IGNORE_FILES = {
    os.path.basename(__file__), # 忽略脚本本身
    OUTPUT_FILENAME,
    '.DS_Store'
}

# ----------------

def is_text_file(filename):
    """判断是否为目标代码文件"""
    if filename in IGNORE_FILES:
        return False
    
    _, ext = os.path.splitext(filename)
    if ext.lower() in ALLOWED_EXTENSIONS:
        return True
    
    if filename in ALLOWED_FILENAMES:
        return True
        
    return False

def pack_project():
    project_root = os.getcwd()
    output_path = os.path.join(project_root, OUTPUT_FILENAME)
    
    with open(output_path, 'w', encoding='utf-8') as outfile:
        # 遍历目录
        for root, dirs, files in os.walk(project_root):
            # 修改 dirs 列表，以原地移除不需要遍历的文件夹
            # 这样 os.walk 就不会进入这些目录，大幅提升速度
            dirs[:] = [d for d in dirs if d not in IGNORE_DIRS]
            
            for file in files:
                if is_text_file(file):
                    file_path = os.path.join(root, file)
                    # 计算相对路径，显示起来更干净
                    rel_path = os.path.relpath(file_path, project_root)
                    
                    print(f"Processing: {rel_path}")
                    
                    try:
                        with open(file_path, 'r', encoding='utf-8') as infile:
                            content = infile.read()
                            
                            # 写入分隔符和文件路径，方便 AI 识别
                            outfile.write("=" * 80 + "\n")
                            outfile.write(f"File: {rel_path}\n")
                            outfile.write("=" * 80 + "\n\n")
                            outfile.write("```\n" + content + "\n```\n\n")
                            
                    except UnicodeDecodeError:
                        print(f"Warning: Could not decode {rel_path}, skipping.")
                    except Exception as e:
                        print(f"Error reading {rel_path}: {e}")

    print(f"\nDone! All code has been packed into: {OUTPUT_FILENAME}")
    print("You can now copy the content of this file to your AI assistant.")

if __name__ == "__main__":
    pack_project()