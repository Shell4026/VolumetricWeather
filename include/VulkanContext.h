#pragma once
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#define VK_RESULT_CHECK(func)\
if (VkResult result = func; result != VkResult::VK_SUCCESS)\
{\
	SH_ERROR_FORMAT("Failed to {}: {}", #func, string_VkResult(result));\
	throw std::runtime_error{ "VkResult is not VK_SUCCESS!" };\
}

#include <vulkan/vulkan.hpp>
#include "vulkan/vk_enum_string_helper.h"

#include <vector>
#include <optional>
class Window;
class VulkanContext
{
public:
	VulkanContext(const Window& window);
	~VulkanContext();

	void Init();
	void ReSize();

	auto FindExtension(const char* name) const -> bool;
	auto AcquireNextImage(VkSemaphore semaphore, VkFence fence, uint32_t* imgIdx) -> bool;
	void BarrierCommand(VkCommandBuffer cmd, VkImage img, VkImageAspectFlags aspect,
		VkImageLayout srcLayout, VkImageLayout dstLayout, 
		VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage, 
		VkAccessFlags srcAccess, VkAccessFlags dstAccess);

	auto FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const -> uint32_t;

	auto GetMemoryTypeIndex(uint32_t memoryTypeBits, VkMemoryPropertyFlags properties) const -> std::optional<uint32_t>;
	auto GetInstance() const -> VkInstance { return instancePtr; }
	auto GetDevice() const -> VkDevice { return device; }
	auto GetPhysicalDevice() const -> VkPhysicalDevice { return gpu; }
	auto GetCommandPool() const -> VkCommandPool { return graphicsCommandPool; }
	auto GetComputeCommandPool() const -> VkCommandPool { return computeCommandPool; }
	auto GetSwapChain() const -> VkSwapchainKHR { return swapChain; }
	auto GetSwapChainImagesFormat() const -> VkFormat { return swapChainImageFormat; }
	auto GetSwapChainDepthFormat() const -> VkFormat { return swapChainDepthFormat; }
	auto GetSwapChainImages() const -> const std::vector<VkImage>& { return swapChainImages; }
	auto GetSwapChainImageViews() const -> const std::vector<VkImageView>& { return swapChainImageViews; }
	auto GetSwapChainDepthImages() const -> const std::vector<VkImage>& { return swapChainDepthImages; }
	auto GetSwapChainDepthImageViews() const -> const std::vector<VkImageView>& { return swapChainDepthImageViews; }
	auto GetSwapChainExtent() const -> const VkExtent2D& { return swapChainExtent; }
	auto GetGraphicsQueueFamily() const -> uint32_t { return graphicsQueueFamily; }
	auto GetGraphicsQueue() const -> VkQueue { return graphicsQueue; }
	auto GetPresentQueue() const -> VkQueue { return presentQueue; }
	auto GetComputeQueueFamily() const -> uint32_t { return computeQueueFamily; }
	auto GetComputeQueue() const -> VkQueue { return computeQueue; }
private:
	void QueryInstanceLayers();
	void CreateDebugInfo();
	void PrepareSurfaceExtension();
	void CreateInstance();
	void InitDebugMessenger();
	void CreateSurface();
	void QueryPhysicalDevice();
	void SelectQueue();
	void CreateDevice();
	void CreateSwapChain();
	void CreateCommandPool();
	void ClearSwapChain();
public:
	static constexpr uint32_t MAX_CONCURRENT_FRAMES{ 2 };
private:
	const Window& window;

	// 인스턴스, 디바이스
	VkInstance instancePtr = VK_NULL_HANDLE;

	std::vector<const char*> reqLayers;
	std::vector<const char*> reqExtensions;
	std::vector<const char*> reqDeviceExtensions;

	VkPhysicalDevice gpu = VK_NULL_HANDLE;
	VkPhysicalDeviceProperties gpuProps;
	VkPhysicalDeviceFeatures gpuFeatures;
	VkPhysicalDeviceMemoryProperties gpuMemProps;

	VkDevice device = VK_NULL_HANDLE;

	// 레이어, 확장
	struct LayerProperty
	{
		VkLayerProperties layer;
		std::vector<VkExtensionProperties> extensions;
	};
	std::vector<LayerProperty> props;
	std::vector<VkExtensionProperties> extensions;

	// 디버거
	VkDebugUtilsMessengerCreateInfoEXT debugInfo{};
	std::vector<VkValidationFeatureEnableEXT> validationEnables;
	VkValidationFeaturesEXT validationFeatures{};
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

	// 스왑체인
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	std::vector<VkSurfaceFormatKHR> surfaceFormats;
	std::vector<VkPresentModeKHR> presentModes;

	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkSwapchainKHR swapChain = VK_NULL_HANDLE;
	
	VkFormat swapChainImageFormat;
	VkFormat swapChainDepthFormat = VkFormat::VK_FORMAT_D24_UNORM_S8_UINT;
	VkExtent2D swapChainExtent;
	std::vector<VkImage> swapChainImages;
	std::vector<VkImage> swapChainDepthImages;
	std::vector<VkDeviceMemory> swapChainDepthMemories;
	std::vector<VkImageView> swapChainImageViews;
	std::vector<VkImageView> swapChainDepthImageViews;

	// 큐
	uint32_t graphicsQueueFamily = 0xFFFFFFFF;
	uint32_t presentQueueFamily = 0xFFFFFFFF;
	uint32_t computeQueueFamily = 0xFFFFFFFF;
	VkQueue graphicsQueue = VK_NULL_HANDLE;
	VkQueue presentQueue = VK_NULL_HANDLE;
	VkQueue computeQueue = VK_NULL_HANDLE;

	// 커맨드 버퍼
	VkCommandPool graphicsCommandPool = VK_NULL_HANDLE;
	VkCommandPool computeCommandPool = VK_NULL_HANDLE;

	bool bEnableValidationLayers = true;
};