# CFG_builder
A C/C++ static analysis tool prototype based on Clang LibTooling, used to generate AST and CFG for functions in a single Translation Unit (TU) and output them in JSON format.

Current Focus: Linux kernel C code (depends on `compile_commands.json`).

---

## 1. Features

- Read compilation arguments from `compile_commands.json`.
- Perform frontend syntax analysis on specified source files (single TU).
- Collect function definitions from the main file (filterable by function name).
- Build function-level CFG.
- Output stable JSON (supports AST+CFG / AST only / CFG only modes).

---

## 2. Project Structure

```
CFG_builder/
├── CMakeLists.txt
├── include/
│   ├── app/
│   │   ├── Config.h            # Config model and CLI interface
│   │   └── ToolRunner.h        # Execution entry (Run / RunAstJson / RunCfgJson)
│   ├── ast/
│   │   └── FunctionCollector.h # Collect function definitions from AST
│   ├── frontend/
│   │   └── CFGFrontendAction.h # Clang Frontend Action
│   ├── output/
│   │   └── CFGPrinter.h        # AST/CFG JSON construction
│   └── ...
├── src/
│   ├── main.cpp
│   └── ...
├── testdata/
│   ├── simple.c
│   └── compile_commands.json
└── scripts/
    └── run_one_tu.sh
```

---

## 3. Requirements

Ubuntu + LLVM/Clang 18 is recommended.

Minimum dependencies:

- `cmake`
- `ninja-build`
- `clang-18` / `clang++-18`
- `llvm-18-dev`
- `libclang-18-dev`

---

## 4. Build

Run in the project root:

```bash
cmake -S . -B build -G Ninja \
	-DCMAKE_C_COMPILER=clang-18 \
	-DCMAKE_CXX_COMPILER=clang++-18 \
	-DLLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm

cmake --build build
```

---

## 5. CLI Usage

### Arguments

- `--compdb <path>`: Path to `compile_commands.json` (Required)
- `--file <path>`: Path to source file (Required)
- `--functions <f1,f2,...>`: Function name filter (Optional)
- `--output <path>`: Write JSON to file (Optional)
- `-h / --help`: Help

### Example 1: Test Data

```bash
./build/cfg_builder \
	--compdb testdata/compile_commands.json \
	--file testdata/simple.c \
	--functions main
```

### Example 2: Output to File

```bash
./build/cfg_builder \
	--compdb testdata/compile_commands.json \
	--file testdata/simple.c \
	--functions sum_until_limit \
	--output /tmp/cfg.json
```

### Example 3: Linux Kernel File

```bash
./build/cfg_builder \
	--compdb /home/niuniu/linux/compile_commands.json \
	--file /home/niuniu/linux/io_uring/io_uring.c \
	--functions io_get_ext_arg_reg
```

---

## 6. JSON Output (Current Version)

Top-level fields:

- `source_file`
- `functions_requested`
- `requested_function_names`
- `functions_found`
- `functions[]`
	- `name`
	- `ast` (when AST mode enabled)
	- `cfg` (when CFG mode enabled)

The `cfg.blocks[]` contains:

- `id`
- `is_entry` / `is_exit`
- `preds[]`
- `succs[]`
- `terminator`
- `stmts[]`

---

## 7. API for Integration (e.g., pybind11)

`ToolRunner` provides three interfaces:

- `Run(...)`: Default output (AST+CFG) for CLI.
- `RunAstJson(...)`: Returns AST JSON string.
- `RunCfgJson(...)`: Returns CFG JSON string.

---

## 8. FAQ

1. **VS Code shows errors but it builds/runs via CLI**
   Usually caused by version mismatch between CMake kits and IntelliSense. Use Clang/LLVM 18 consistently.
2. **Kernel files report unknown warning/argument**
   Clang might not recognize certain GCC-specific flags in the DB. Rebuilding the kernel with Clang to generate the DB is recommended.
3. **`--file` returns no results**
   Ensure the file path matches the one in `compile_commands.json` exactly (use absolute paths if possible).

---

# CFG_builder

基于 Clang LibTooling 的 C/C++ 静态分析工具原型，用于对单个编译单元（TU）中的函数生成 AST 与 CFG，并以 JSON 形式输出。

当前重点场景：Linux 内核 C 代码（依赖 `compile_commands.json`）。

---

## 1. 当前能力

- 读取 `compile_commands.json` 的编译参数。
- 对指定源文件（单 TU）做语法前端分析。
- 收集主文件中函数定义（可按函数名过滤）。
- 构建函数级 CFG。
- 输出稳定 JSON（支持 AST+CFG / 仅 AST / 仅 CFG 三种内部模式）。

---

## 2. 项目结构

```
CFG_builder/
├── CMakeLists.txt
├── include/
│   ├── app/
│   │   ├── Config.h            # 参数模型与命令行解析接口
│   │   └── ToolRunner.h        # 执行入口（Run / RunAstJson / RunCfgJson）
│   ├── ast/
│   │   └── FunctionCollector.h # 从 AST 收集函数定义
│   ├── frontend/
│   │   └── CFGFrontendAction.h # Clang 前端动作，组织分析流程
│   ├── output/
│   │   └── CFGPrinter.h        # AST/CFG JSON 构造
│   ├── cfg/
│   │   └── CFGBuilderService.h # 预留模块
│   └── common/
│       └── Diagnostics.h       # 预留模块
├── src/
│   ├── main.cpp
│   ├── app/
│   │   ├── Config.cpp
│   │   └── ToolRunner.cpp
│   ├── ast/
│   │   └── FunctionCollector.cpp
│   ├── frontend/
│   │   └── CFGFrontendAction.cpp
│   ├── output/
│   │   └── CFGPrinter.cpp
│   ├── cfg/
│   │   └── CFGBuilderService.cpp
│   └── common/
│       └── Diagnostics.cpp
├── testdata/
│   ├── simple.c
│   └── compile_commands.json
└── scripts/
		└── run_one_tu.sh
```

---

## 3. 环境要求

建议 Ubuntu + LLVM/Clang 18。

最小依赖示例：

- `cmake`
- `ninja-build`
- `clang-18` / `clang++-18`
- `llvm-18-dev`
- `libclang-18-dev`

---

## 4. 构建

在项目根目录执行：

```bash
cmake -S . -B build -G Ninja \
	-DCMAKE_C_COMPILER=clang-18 \
	-DCMAKE_CXX_COMPILER=clang++-18 \
	-DLLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm

cmake --build build
```

---

## 5. 命令行使用

### 参数

- `--compdb <path>`：`compile_commands.json` 路径（必选）
- `--file <path>`：源文件路径（必选）
- `--functions <f1,f2,...>`：函数名过滤（可选）
- `--output <path>`：将 JSON 写入文件（可选）
- `-h / --help`：帮助

### 示例 1：测试数据

```bash
./build/cfg_builder \
	--compdb testdata/compile_commands.json \
	--file testdata/simple.c \
	--functions main
```

### 示例 2：输出到文件

```bash
./build/cfg_builder \
	--compdb testdata/compile_commands.json \
	--file testdata/simple.c \
	--functions sum_until_limit \
	--output /tmp/cfg.json
```

### 示例 3：Linux 内核文件

```bash
./build/cfg_builder \
	--compdb /home/niuniu/linux/compile_commands.json \
	--file /home/niuniu/linux/io_uring/io_uring.c \
	--functions io_get_ext_arg_reg
```

---

## 6. JSON 输出说明（当前版本）

顶层字段示意：

- `source_file`
- `functions_requested`
- `requested_function_names`
- `functions_found`
- `functions[]`
	- `name`
	- `ast`（当模式包含 AST）
	- `cfg`（当模式包含 CFG）

其中 `cfg.blocks[]` 包含：

- `id`
- `is_entry` / `is_exit`
- `preds[]`
- `succs[]`
- `terminator`
- `stmts[]`

---

## 7. 代码接口（供 pybind11 等调用）

`ToolRunner` 已提供三个接口：

- `Run(...)`：默认输出 AST+CFG（CLI 使用）
- `RunAstJson(...)`：仅 AST JSON 字符串
- `RunCfgJson(...)`：仅 CFG JSON 字符串

适合直接封装为 Python 扩展函数。

---

## 8. 常见问题

1. **VS Code 有红线但命令行可运行**  
	 多半是 CMake/IntelliSense 使用了不同 LLVM 版本。请统一到 clang/llvm-18。

2. **内核文件报大量 unknown warning/argument**  
	 常见于 GCC 编译参数被 Clang 前端复用。建议用 Clang 重新生成内核编译数据库。

3. **`--file` 无结果**  
	 确认该文件存在于 `compile_commands.json`，并尽量使用绝对路径。

---