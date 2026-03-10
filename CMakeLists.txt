cmake_minimum_required(VERSION 3.20)

# 本文件用于配置项目构建规则与依赖。
project(CFGBuilder LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 阶段 A：先验证 LLVM/Clang 开发环境可被 CMake 正确发现。
find_package(LLVM REQUIRED CONFIG)

# 统一 LLVM/Clang 版本：始终从 LLVM_DIR 推导 ClangConfig 位置。
get_filename_component(_llvm_cmake_parent "${LLVM_DIR}" DIRECTORY)
set(_clang_hint_dir "${_llvm_cmake_parent}/clang")
if(EXISTS "${_clang_hint_dir}/ClangConfig.cmake")
	set(Clang_DIR "${_clang_hint_dir}" CACHE PATH "Path to ClangConfig.cmake directory" FORCE)
endif()

find_package(Clang REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "Using ClangConfig.cmake in: ${Clang_DIR}")

include_directories(
	${LLVM_INCLUDE_DIRS}
	${CLANG_INCLUDE_DIRS}
)
add_definitions(${LLVM_DEFINITIONS})

# 阶段 A：先构建骨架库，再提供最小可执行程序用于运行验证。
add_library(cfg_builder_skeleton STATIC
	src/app/Config.cpp
	src/app/ToolRunner.cpp
	src/frontend/CFGFrontendAction.cpp
	src/ast/FunctionCollector.cpp
	src/cfg/CFGBuilderService.cpp
	src/output/CFGPrinter.cpp
	src/common/Diagnostics.cpp
)

target_include_directories(cfg_builder_skeleton
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(cfg_builder_skeleton
	PUBLIC
		clang-cpp
		LLVM
)

add_executable(cfg_builder
	src/main.cpp
)

target_link_libraries(cfg_builder
	PRIVATE
		cfg_builder_skeleton
)