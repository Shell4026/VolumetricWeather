cmake_minimum_required (VERSION 3.8)

include(FetchContent)

project ("VolumetricWeather" LANGUAGES CXX)

file(GLOB spvFiles "${CMAKE_SOURCE_DIR}/resource/*.spv")
file(COPY ${spvFiles} DESTINATION ${CMAKE_BINARY_DIR}/shaders)
file(GLOB glbFiles "${CMAKE_SOURCE_DIR}/resource/*.glb")
file(COPY ${glbFiles} DESTINATION ${CMAKE_BINARY_DIR}/models)
file(GLOB pngFiles "${CMAKE_SOURCE_DIR}/resource/*.png")
file(COPY ${pngFiles} DESTINATION ${CMAKE_BINARY_DIR}/textures)
file(GLOB binFiles "${CMAKE_SOURCE_DIR}/resource/*.bin")
file(COPY ${binFiles} DESTINATION ${CMAKE_BINARY_DIR}/textures)
file(GLOB fontFiles "${CMAKE_SOURCE_DIR}/resource/*.otf")
file(COPY ${fontFiles} DESTINATION ${CMAKE_BINARY_DIR}/fonts)
file(GLOB presetFiles "${CMAKE_SOURCE_DIR}/resource/*.json")
file(COPY ${presetFiles} DESTINATION ${CMAKE_BINARY_DIR})
message("Asset copy is completed")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)
set(CORE_HEADERS "include/core/Event.h" "include/core/ImGUI.h" "include/core/Input.h" "include/core/Logger.h" "include/core/Window.h" "include/core/CircularQueue.hpp" "include/core/Util.h" "include/core/Noise.h" "include/core/ISerializable.h")
set(CORE_CPPS "src/core/Event.cpp" "src/core/ImGUI.cpp" "src/core/Input.cpp" "src/core/Logger.cpp" "src/core/Window.cpp" "src/core/Implemntation.cpp" "src/core/Util.cpp" "src/core/Noise.cpp")
set(PASS_HEADERS 
"include/pass/APass.h"
"include/pass/PostProcessPass.h"
"include/pass/OpaquePass.h"
"include/pass/ShadowPass.h"
"include/pass/IDrawablePass.h"
"include/pass/LUTPass.h"
"include/pass/AtmospherePass.h"
"include/pass/HillairePass.h"
"include/pass/BlitPass.h" 
"include/pass/CloudPass.h"
"include/pass/CloudTRPass.h"
"include/pass/LowDepthPass.h"
"include/pass/CloudPaintPass.h"
 )
set(PASS_CPPS 
"src/pass/APass.cpp"
"src/pass/PostProcessPass.cpp"
"src/pass/OpaquePass.cpp"
"src/pass/ShadowPass.cpp"
"src/pass/LUTPass.cpp"
"src/pass/AtmospherePass.cpp"
"src/pass/HillairePass.cpp"
"src/pass/BlitPass.cpp"
"src/pass/CloudPass.cpp"
"src/pass/CloudTRPass.cpp"
"src/pass/LowDepthPass.cpp"
"src/pass/CloudPaintPass.cpp"
)
set(WEATHER_HEADERS
"include/weather/Setting.h"
"include/weather/AtmosphereRMSEMeasurement.h"
"include/weather/ParameterMapper.h"
"include/weather/ArtistGUI.h"
"include/weather/IWeatherPass.h"
)
set(WEATHER_CPPS
"src/weather/AtmosphereRMSEMeasurement.cpp"
"src/weather/ParameterMapper.cpp"
"src/weather/ArtistGUI.cpp"
 "src/weather/Setting.cpp")
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
"include/render/SamplerManager.h"
"include/render/Camera.h"
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
"src/render/SamplerManager.cpp"
"src/render/Camera.cpp"
)
set(HEADERS 
"include/FPSCamera.h"
"include/Scene.h"
"include/GLBLoader.h"
"include/BasisScene.h" 
"include/PresetManager.h"
"include/TextureLoader.h"
"include/SceneManager.h"
"include/CloudEditor.h" 
"include/GUIBase.h" "include/IGUI.h" 
"include/CurveEditor.h"
"include/Bezier.hpp"
)
set(CPPS
"src/FPSCamera.cpp"
"src/GLBLoader.cpp"
"src/Scene.cpp"
"src/BasisScene.cpp"
"src/PresetManager.cpp"
"src/TextureLoader.cpp"
"src/SceneManager.cpp"
"src/CloudEditor.cpp"
"src/GUIBase.cpp"
"src/CurveEditor.cpp")
add_executable (VolumetricWeather "main.cpp" "include/PCH.h" ${CORE_CPPS} ${CORE_HEADERS} ${CPPS} ${HEADERS} ${PASS_CPPS} ${PASS_HEADERS} ${RENDER_CPPS} ${RENDER_HEADERS} ${WEATHER_CPPS} ${WEATHER_HEADERS})

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

FETCHCONTENT_DECLARE(
	JSON
	GIT_REPOSITORY "https://github.com/nlohmann/json"
	GIT_TAG "v3.11.3"
)
FETCHCONTENT_MAKEAVAILABLE(JSON)

target_precompile_headers(VolumetricWeather PRIVATE "include/PCH.h")
target_link_libraries(VolumetricWeather PRIVATE Vulkan::Vulkan)
target_link_libraries(VolumetricWeather PRIVATE glm)
target_link_libraries(VolumetricWeather PRIVATE IMGUI)
target_link_libraries(VolumetricWeather PRIVATE nlohmann_json)
target_include_directories(VolumetricWeather PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(VolumetricWeather PRIVATE ${CMAKE_SOURCE_DIR}/external)
add_subdirectory(external/imgui)
