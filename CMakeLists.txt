cmake_minimum_required(VERSION 3.15)
project(OpenGarden)
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

# 下载并配置 GLFW、ImGui
include(FetchContent)

FetchContent_Declare(glfw    SOURCE_DIR "${CMAKE_SOURCE_DIR}/include/glfw-3.3.8")
FetchContent_Declare(imgui   SOURCE_DIR "${CMAKE_SOURCE_DIR}/include/imgui-1.90.4")

FetchContent_MakeAvailable(glfw imgui)

# GLEW源码内联（静态编译）----
set(GLEW_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/include/glew-2.2.0)
set(GLEW_SOURCES ${GLEW_ROOT}/src/glew.c)

add_library(glew STATIC ${GLEW_SOURCES})
target_include_directories(glew PUBLIC ${GLEW_ROOT}/include)
target_compile_definitions(glew PRIVATE GLEW_STATIC)
set_target_properties(glew PROPERTIES LINKER_LANGUAGE C)

# 添加 Assimp 子目录
add_subdirectory("${CMAKE_SOURCE_DIR}/include/assimp")

# 添加 ImGui 源文件
set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
set(IMGUI_HEADERS
    ${imgui_SOURCE_DIR}/imgui.h
    ${imgui_SOURCE_DIR}/imgui_internal.h
    ${imgui_SOURCE_DIR}/imconfig.h
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.h
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.h
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3_loader.h
)

# 项目源文件
set(PROJECT_SOURCES
  src/main.cpp
  # src/main1.cpp  # 如果不需要可删
  ${IMGUI_SOURCES}
  ${GLEW_SOURCES}
)

# 创建可执行文件
add_executable(${PROJECT_NAME} 
    "src/main.cpp" 
    ${IMGUI_SOURCES}
)

# ================= 路径宏定义 =================
# 工程根目录
target_compile_definitions(${PROJECT_NAME} PRIVATE
    PROJECT_ROOT="${CMAKE_SOURCE_DIR}"
)

# 资源目录
target_compile_definitions(${PROJECT_NAME} PRIVATE
    ASSETS_FOLDER="${CMAKE_SOURCE_DIR}/src/assets/"
)

# Shader 目录
target_compile_definitions(${PROJECT_NAME} PRIVATE
    SHADERS_FOLDER="${CMAKE_SOURCE_DIR}/src/shaders/"
)


# 包含目录
target_include_directories(${PROJECT_NAME} PRIVATE
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${GLEW_ROOT}/include
    ${CMAKE_SOURCE_DIR}/include/assimp/include
)
include_directories("${CMAKE_SOURCE_DIR}/include")
include_directories("${CMAKE_SOURCE_DIR}/include/tinygltf")
# 链接库
target_link_libraries(${PROJECT_NAME} PRIVATE
    OpenGL::GL
    glfw
    glew
    assimp
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
# 复制 DLL 文件（Windows）
if (WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:glfw>"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMENT "Copying GLFW DLL to output directory"
    )
endif()
# 设置调试和发布模式的编译选项
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