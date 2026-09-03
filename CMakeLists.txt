cmake_minimum_required(VERSION 3.10)
project(helloCAD)

# 设置全局 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(helloCAD
    src/main.cpp
    src/scene/Transform.cpp
    src/view/OrbitCamera.cpp
    src/view/Controller.cpp
    src/geometry/OcctMesher.cpp
    src/render/ShaderProgram.cpp
    src/render/GpuMesh.cpp
    src/asset/StepXcafImporter.cpp
)

add_library(glad STATIC
    src/glad/glad.c
)

set(IMGUI_DIR ${PROJECT_SOURCE_DIR}/External/imgui)
# 核心源文件
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
)

# 如果有 backends（根据你的需求选择）
set(IMGUI_BACKEND_SOURCES
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
add_library(imgui STATIC
    ${IMGUI_SOURCES} 
    ${IMGUI_BACKEND_SOURCES}
)


# 设置 GLFW 路径为缓存变量
set(GLFW_ROOT "${PROJECT_SOURCE_DIR}/External/glfw-3.4" CACHE PATH "GLFW root directory")
set(OCCT_ROOT "C:/Users/28025/Learn/Geometry/OCCT-8.0.1/install-debug")

find_package(OpenCASCADE 8.0.1 CONFIG REQUIRED
    COMPONENTS
        FoundationClasses
        ModelingData
        ModelingAlgorithms
        Visualization
        ApplicationFramework
        DataExchange
    PATHS
        "${OCCT_ROOT}/cmake"
    NO_DEFAULT_PATH
)

target_include_directories(helloCAD PRIVATE
    "${GLFW_ROOT}/include"
)

target_include_directories(glad PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

target_include_directories(imgui PUBLIC
    ${GLFW_ROOT}/include
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)
target_link_directories(imgui PUBLIC 
    ${GLFW_ROOT}/lib-vc2022
)
target_compile_definitions(imgui PUBLIC GLFW_DLL)

target_link_libraries(imgui PUBLIC
    glad
    glfw3dll
)

# 链接 GLFW 静态或动态库
target_link_directories(helloCAD PRIVATE 
    ${GLFW_ROOT}/lib-vc2022
)

# 编译时定义 DLL 模式（告诉 GLFW 不要用静态路径）
target_compile_definitions(helloCAD PRIVATE GLFW_DLL)

# 链接 OpenGL 和 GLFW
target_link_libraries(helloCAD PRIVATE
    TKernel
    TKMath
    TKBRep
    TKTopAlgo
    TKMesh
    TKPrim
    TKCDF
    TKLCAF
    TKCAF
    TKXCAF
    TKBinXCAF
    TKDE
    TKXSBase
    TKDESTEP
    TKXml
    glad
    glfw3dll 
    opengl32
    imgui
)

# 构建后自动复制 DLL（非常推荐）
add_custom_command(
    TARGET helloCAD POST_BUILD

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${GLFW_ROOT}/lib-vc2022/glfw3.dll"
        "$<TARGET_FILE_DIR:helloCAD>"

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:helloCAD>
        $<TARGET_FILE_DIR:helloCAD>

    COMMAND_EXPAND_LISTS
)

set(MYCAD_RUNTIME_DIR
    "${CMAKE_BINARY_DIR}/$<CONFIG>"
)

add_custom_target(copy_shaders ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${MYCAD_RUNTIME_DIR}/shader"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/shader"
        "${MYCAD_RUNTIME_DIR}/shader"
    VERBATIM
)

add_dependencies(helloCAD copy_shaders)