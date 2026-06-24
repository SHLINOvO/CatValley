cmake_minimum_required(VERSION 3.15)
project(CatValley)
# 设置 CMake 策略以消除警告
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 设置编译器模式（Debug/Release）
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()
# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 查找 OpenGL
find_package(OpenGL REQUIRED)

# ================= GLFW（预编译二进制版） =================
set(GLFW_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/glfw-3.4.bin.WIN64)
set(GLFW_INCLUDE_DIR ${GLFW_ROOT}/include)
set(GLFW_LIB_DIR ${GLFW_ROOT}/lib-vc2022)

# ================= GLEW（预编译静态版） =================
set(GLEW_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/glew-2.3.1)
set(GLEW_INCLUDE_DIR ${GLEW_ROOT}/include)
set(GLEW_LIB_DIR ${GLEW_ROOT}/lib/Release/x64)

# ================= ImGui（本地源码） =================
include(FetchContent)
FetchContent_Declare(imgui SOURCE_DIR "${CMAKE_SOURCE_DIR}/include/imgui-1.90.4")
FetchContent_MakeAvailable(imgui)

set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

# ================= Assimp（本地源码子目录编译） =================
# Assimp 配置选项：减少编译时间
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_VIEW OFF CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory("${CMAKE_SOURCE_DIR}/include/assimp")

# ================= 项目源文件 =================
set(PROJECT_SOURCES
    src/main.cpp
    src/glad.c
    ${IMGUI_SOURCES}
)

# 创建可执行文件
add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

# ================= 路径宏定义 =================
# 使用相对路径，这样把整个输出目录打包发给别人也能直接运行
target_compile_definitions(${PROJECT_NAME} PRIVATE
    PROJECT_ROOT="."
    ASSETS_FOLDER="assets/"
    SHADERS_FOLDER="shaders/"
)

# ================= 包含目录 =================
target_include_directories(${PROJECT_NAME} PRIVATE
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${GLEW_INCLUDE_DIR}
    ${GLFW_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/include/assimp/include
    ${CMAKE_BINARY_DIR}/include  # assimp 生成的 config.h
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/glm  # GLM: 仓库根目录，#include <glm/glm.hpp> 对应 glm/glm.hpp
    ${CMAKE_SOURCE_DIR}/src/include
)

# ================= 链接库 =================
# GLFW (预编译)
target_link_libraries(${PROJECT_NAME} PRIVATE
    OpenGL::GL
    glfw3
    glew32s
    assimp
)

# 添加 GLFW 和 GLEW 库搜索路径
target_link_directories(${PROJECT_NAME} PRIVATE
    ${GLFW_LIB_DIR}
    ${GLEW_LIB_DIR}
)

# 在 Windows 上链接额外的库
if (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        opengl32
    )
endif()

# 设置编译定义
target_compile_definitions(${PROJECT_NAME} PRIVATE
    GLEW_STATIC
    IMGUI_IMPL_OPENGL_LOADER_GLEW
)

# 复制 DLL 文件到输出目录
if (WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${GLFW_LIB_DIR}/glfw3.dll"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMENT "Copying GLFW DLL to output directory"
    )
    # Assimp DLL (根据构建类型选择 Debug 或 Release 版)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<IF:$<CONFIG:Debug>,${CMAKE_BINARY_DIR}/bin/Debug/assimp-vc145-mtd.dll,${CMAKE_BINARY_DIR}/bin/Release/assimp-vc145-mt.dll>"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMENT "Copying Assimp DLL to output directory"
    )
    # VC++ 运行时 DLL（让没装 VC++ Redistributable 的电脑也能运行）
    foreach(_vcdll MSVCP140 VCRUNTIME140 VCRUNTIME140_1)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$ENV{SystemRoot}/System32/${_vcdll}.dll"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            COMMENT "Copying ${_vcdll}.dll to output directory"
        )
    endforeach()
endif()

# 复制 assets 和 shaders 到输出目录（用于独立分发）
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/src/assets"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    COMMENT "Copying assets to output directory"
)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/src/shaders"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders"
    COMMENT "Copying shaders to output directory"
)

# 设置编译选项
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:/Od>
        $<$<CONFIG:Release>:/O2>
    )
endif()

# 添加编译特性要求
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

message(STATUS "Project configuration complete!")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Output directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "GLFW: ${GLFW_ROOT}")
message(STATUS "GLEW: ${GLEW_ROOT}")
message(STATUS "ImGui: ${imgui_SOURCE_DIR}")
message(STATUS "Assimp: include/assimp")
