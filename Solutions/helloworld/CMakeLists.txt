# CMakeList.txt: helloworld에 대한 CMake 프로젝트, 여기에 소스를 포함하고
# 프로젝트 특정 논리를 정의합니다.
#
cmake_minimum_required (VERSION 3.21)
project (helloworld)

set(CMAKE_CXX_STANDARD 17)
set(glfw3_DIR "C:/vcpkg-master/installed/x64-windows/share/glfw3")

# Vulcan SDK 환경변수에서 가져오기
find_package(glfw3 REQUIRED)
find_package(Vulkan REQUIRED)

# SHADER_SOURCE_DIR는 쉐이더 파일들이 있는 원본 디렉토리입니다.
set(SHADER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/shaders")


# 이 프로젝트의 실행 파일에 소스를 추가합니다.
add_executable (helloworld "main.cpp" "VulkanUtils.h" "VulkanUtils.cpp")
target_link_libraries(helloworld PRIVATE glfw Vulkan::Vulkan)

# helloworld 실행 파일이 생성되는 디렉토리 (e.g., F:\Vulkan\Solutions\helloworld\out\build\x64-debug\Debug)
set(EXE_OUTPUT_DIR "$<TARGET_FILE_DIR:helloworld>")

add_custom_command(
    TARGET helloworld POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${EXE_OUTPUT_DIR}/shaders" # 목적지 'shaders' 폴더 생성
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SHADER_SOURCE_DIR}/vert.spv"
            "${EXE_OUTPUT_DIR}/shaders/vert.spv"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SHADER_SOURCE_DIR}/frag.spv"
            "${EXE_OUTPUT_DIR}/shaders/frag.spv"
    VERBATIM
)

# ImGui 경로(예시: 프로젝트/thirdparty/imgui)
set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/thirdparty/imgui")

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)

set(IMGUI_HEADERS
    ${IMGUI_DIR}/imgui.h
    ${IMGUI_DIR}/imgui_internal.h
    ${IMGUI_DIR}/imconfig.h
    ${IMGUI_DIR}/imstb_rectpack.h
    ${IMGUI_DIR}/imstb_textedit.h
    ${IMGUI_DIR}/imstb_truetype.h
    ${IMGUI_DIR}/backends/imgui_impl_glfw.h
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.h
)

# 실행 파일에 소스 포함
target_sources(helloworld PRIVATE ${IMGUI_SOURCES})
target_include_directories(helloworld PRIVATE ${IMGUI_DIR} ${IMGUI_DIR}/backends)

target_compile_definitions(helloworld PRIVATE IMGUI_IMPL_VULKAN_NO_PROTOTYPES)