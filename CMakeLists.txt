cmake_minimum_required (VERSION 3.8)

project ("VolumetricWeather" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)

add_executable (VolumetricWeather "main.cpp" "include/Logger.h" "src/Logger.cpp" "src/VulkanContext.cpp" "include/VulkanContext.h" "include/Window.h" "src/Window.cpp" "include/PCH.h" "src/Scene.cpp" "include/Scene.h")

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

target_precompile_headers(VolumetricWeather PRIVATE "include/PCH.h")
target_link_libraries(VolumetricWeather PRIVATE Vulkan::Vulkan)
target_include_directories(VolumetricWeather PRIVATE ${CMAKE_SOURCE_DIR}/include)

file(GLOB spvFiles "${CMAKE_SOURCE_DIR}/resource/*.spv")
file(COPY ${spvFiles} DESTINATION ${CMAKE_BINARY_DIR}/shaders)