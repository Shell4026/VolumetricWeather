cmake_minimum_required (VERSION 3.8)

include(FetchContent)

project ("VolumetricWeather" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)
set(CORE_HEADERS "include/core/Event.h" "include/core/ImGUI.h" "include/core/Input.h" "include/core/Logger.h" "include/core/Window.h" "include/core/CircularQueue.hpp")
set(CORE_CPPS "src/core/Event.cpp" "src/core/ImGUI.cpp" "src/core/Input.cpp" "src/core/Logger.cpp" "src/core/Window.cpp")
set(PASS_HEADERS 
"include/pass/APass.h"
"include/pass/AtmospherePass.h"
"include/pass/PostProcessPass.h"
"include/pass/OpaquePass.h"
"include/pass/ShadowPass.h"
"include/pass/IDrawablePass.h"
"include/pass/LUTPass.h"
 "include/pass/HillairePass.h")
set(PASS_CPPS 
"src/pass/APass.cpp"
"src/pass/AtmospherePass.cpp"
"src/pass/PostProcessPass.cpp"
"src/pass/OpaquePass.cpp"
"src/pass/ShadowPass.cpp"
"src/pass/LUTPass.cpp"
 "src/pass/HillairePass.cpp")
set(RENDER_HEADERS 
"include/render/BarrierBuilder.h"
"include/render/BarrierInfo.h"
"include/render/Drawable.hpp"
"include/render/FrameContext.h"
"include/render/GPUTimer.h"
"include/render/Material.h"
"include/render/Mesh.h"
"include/render/Shader.h" 
"include/render/VulkanBuffer.h"
"include/render/VulkanContext.h"
"include/render/VulkanImage.h"
)
set(RENDER_CPPS 
"src/render/BarrierBuilder.cpp"
"src/render/GPUTimer.cpp"
"src/render/Material.cpp"
"src/render/Mesh.cpp"
"src/render/Shader.cpp"
"src/render/VulkanBuffer.cpp"
"src/render/VulkanContext.cpp"
"src/render/VulkanImage.cpp"
)
set(HEADERS 
"include/Camera.h"
"include/FPSCamera.h"
"include/Scene.h"
"include/GLBLoader.h"
 "include/BasisScene.h")
set(CPPS
"src/Camera.cpp"
"src/FPSCamera.cpp"
"src/GLBLoader.cpp"
"src/Scene.cpp"
 "src/BasisScene.cpp")
add_executable (VolumetricWeather "main.cpp" "include/PCH.h" ${CORE_CPPS} ${CORE_HEADERS} ${CPPS} ${HEADERS} ${PASS_CPPS} ${PASS_HEADERS} ${RENDER_CPPS} ${RENDER_HEADERS})

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(VolumetricWeather PRIVATE SH_DEBUG)
endif()

find_package(Vulkan)

if (NOT Vulkan_FOUND)
	message("Vulkan not found")
	if (WIN32)
		set(VULKAN_SDK_DIR "C:/VulkanSDK/1.4.350.0")
		set(Vulkan_INCLUDE_DIR "${VULKAN_SDK_DIR}/Include")
		set(Vulkan_LIBRARY_DIR "${VULKAN_SDK_DIR}/Lib")
	endif()
else()
	message("Vulkan found!")
endif()

FetchContent_Declare(
  GLM
  GIT_REPOSITORY "https://github.com/g-truc/glm"
  GIT_TAG "1.0.1"
  )
FetchContent_MakeAvailable(GLM)

target_precompile_headers(VolumetricWeather PRIVATE "include/PCH.h")
target_link_libraries(VolumetricWeather PRIVATE Vulkan::Vulkan)
target_link_libraries(VolumetricWeather PRIVATE glm)
target_link_libraries(VolumetricWeather PRIVATE IMGUI)
target_include_directories(VolumetricWeather PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(VolumetricWeather PRIVATE ${CMAKE_SOURCE_DIR}/external)
file(GLOB spvFiles "${CMAKE_SOURCE_DIR}/resource/*.spv")
file(COPY ${spvFiles} DESTINATION ${CMAKE_BINARY_DIR}/shaders)
file(GLOB glbFiles "${CMAKE_SOURCE_DIR}/resource/*.glb")
file(COPY ${glbFiles} DESTINATION ${CMAKE_BINARY_DIR}/models)
add_subdirectory(external/imgui)