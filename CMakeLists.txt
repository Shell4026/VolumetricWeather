cmake_minimum_required (VERSION 3.8)

include(FetchContent)

project ("VolumetricWeather" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)
set(CORE_HEADERS "include/core/Event.h" "include/core/ImGUI.h" "include/core/Input.h" "include/core/Logger.h" "include/core/Window.h")
set(CORE_CPPS "src/core/Event.cpp" "src/core/ImGUI.cpp" "src/core/Input.cpp" "src/core/Logger.cpp" "src/core/Window.cpp")
set(PASS_HEADERS "include/pass/APass.h" "include/pass/AtmospherePass.h" "include/pass/CompositePass.h" "include/pass/OpaquePass.h")
set(PASS_CPPS "src/pass/APass.cpp" "src/pass/AtmospherePass.cpp" "src/pass/CompositePass.cpp" "src/pass/OpaquePass.cpp")
set(HEADERS 
"include/Camera.h"
"include/FPSCamera.h"
"include/FrameContext.h"
"include/GPUTimer.h"
"include/Mesh.h"
"include/Scene.h"
"include/VulkanBuffer.h"
"include/VulkanContext.h"
"include/VulkanImage.h"
"include/GLBLoader.h"
"include/Shader.h")
set(CPPS
"src/Camera.cpp"
"src/FPSCamera.cpp"
"src/GPUTimer.cpp"
"src/Mesh.cpp"
"src/Scene.cpp"
"src/VulkanBuffer.cpp"
"src/VulkanContext.cpp"
"src/VulkanImage.cpp"
"src/GLBLoader.cpp"
"src/Shader.cpp")
add_executable (VolumetricWeather "main.cpp" "include/PCH.h" ${CORE_CPPS} ${CORE_HEADERS} ${CPPS} ${HEADERS} ${PASS_CPPS} ${PASS_HEADERS})

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