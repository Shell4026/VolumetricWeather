#include "VulkanContext.h"
#include "Logger.h"
#include "Window.h"

#include <algorithm>
namespace
{
    VKAPI_ATTR auto VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) -> VkBool32
    {
        if (messageSeverity == VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
            std::cout << "VulkanError: " << pCallbackData->pMessage << '\n';
        else if (messageSeverity == VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
            std::cout << "VulkanWarning: " << pCallbackData->pMessage << '\n';
        else
            std::cout << "VulkanInfo: " << pCallbackData->pMessage << '\n';
        return VK_FALSE;
    }
}

VulkanContext::VulkanContext(const Window& window) :
    window(window)
{
    reqLayers =
    {
        "VK_LAYER_KHRONOS_validation"
    };
    reqExtensions =
    {
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
    };

    reqDeviceExtensions =
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
    };
}

VulkanContext::~VulkanContext()
{
    if (instancePtr == nullptr)
        return;

    if (graphicsCommandPool == computeCommandPool)
        computeCommandPool = VK_NULL_HANDLE;
    else
    {
        vkDestroyCommandPool(device, computeCommandPool, nullptr);
        computeCommandPool = VK_NULL_HANDLE;
    }
    vkDestroyCommandPool(device, graphicsCommandPool, nullptr);
    graphicsCommandPool = VK_NULL_HANDLE;

    ClearSwapChain();

    vkDestroyDevice(device, nullptr);
    device = VK_NULL_HANDLE;

    vkDestroySurfaceKHR(instancePtr, surface, nullptr);
    surface = VK_NULL_HANDLE;

    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instancePtr, "vkDestroyDebugUtilsMessengerEXT");
    assert(func != nullptr);
    func(instancePtr, debugMessenger, nullptr);
    debugMessenger = VK_NULL_HANDLE;

    vkDestroyInstance(instancePtr, nullptr);
    instancePtr = VK_NULL_HANDLE;
}

void VulkanContext::Init()
{
    QueryInstanceLayers();
    CreateDebugInfo();
    PrepareSurfaceExtension();
    CreateInstance();
    InitDebugMessenger();
    CreateSurface();
    QueryPhysicalDevice();
    if (gpu == VK_NULL_HANDLE)
    {
        SH_ERROR("GPU is null!");
        throw std::runtime_error{ "GPU is null!" };
    }
    SelectQueue();
    CreateDevice();
    CreateSwapChain();
    CreateCommandPool();
}

void VulkanContext::ReSize()
{
    vkDeviceWaitIdle(device);
    ClearSwapChain();
    CreateSwapChain();
}

auto VulkanContext::FindExtension(const char* name) const -> bool
{
    for (const VkExtensionProperties& prop : extensions)
    {
        if (std::strcmp(prop.extensionName, name) == 0)
            return true;
    }
    return false;
}

auto VulkanContext::AcquireNextImage(VkSemaphore semaphore, VkFence fence, uint32_t* imgIdx) -> bool
{
    VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, semaphore, fence, imgIdx);
    if (result == VkResult::VK_SUBOPTIMAL_KHR || result == VkResult::VK_ERROR_OUT_OF_DATE_KHR)
    {
        ReSize();
        return false;
    }
    return true;
}

void VulkanContext::BarrierCommand(VkCommandBuffer cmd, VkImage img, VkImageAspectFlags aspect, 
    VkImageLayout srcLayout, VkImageLayout dstLayout, 
    VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage, 
    VkAccessFlags srcAccess, VkAccessFlags dstAccess)
{
    VkImageMemoryBarrier barrier{};
    barrier.sType = VkStructureType::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;
    barrier.oldLayout = srcLayout;
    barrier.newLayout = dstLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange = { aspect, 0, 1, 0, 1 };
    barrier.image = img;

    vkCmdPipelineBarrier(
        cmd,
        srcStage,
        dstStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier);
}


auto VulkanContext::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const -> uint32_t
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(gpu, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }

    SH_ERROR("Failed to find suitable memory type!");
    throw std::runtime_error("Failed to find suitable memory type!");
}

auto VulkanContext::GetMemoryTypeIndex(uint32_t memoryTypeBits, VkMemoryPropertyFlags properties) const -> std::optional<uint32_t>
{
    for (uint32_t i = 0; i < gpuMemProps.memoryTypeCount; i++)
    {
        if ((memoryTypeBits & 1) == 1)
        {
            if ((gpuMemProps.memoryTypes[i].propertyFlags & properties) == properties)
                return i;
        }
        memoryTypeBits >>= 1;
    }
    return {};
}

void VulkanContext::QueryInstanceLayers()
{
    std::vector<VkLayerProperties> layerProps;

    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    props.reserve(layerCount);
    layerProps.resize(layerCount);

    vkEnumerateInstanceLayerProperties(&layerCount, layerProps.data());
    for (const VkLayerProperties& layer : layerProps)
    {
        LayerProperty& layerProp = props.emplace_back();
        layerProp.layer = layer;

        uint32_t extensionCount = 0;
        VkResult result;
        result = vkEnumerateInstanceExtensionProperties(layer.layerName, &extensionCount, nullptr);
        assert(result == VkResult::VK_SUCCESS);
        layerProp.extensions.resize(extensionCount);
        result = vkEnumerateInstanceExtensionProperties(layer.layerName, &extensionCount, layerProp.extensions.data());
        assert(result == VkResult::VK_SUCCESS);
    }
    uint32_t extensionCount = 0;
    VkResult result = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    assert(result == VkResult::VK_SUCCESS);

    extensions.resize(extensionCount);

    result = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
    assert(result == VkResult::VK_SUCCESS);
}

void VulkanContext::CreateDebugInfo()
{
    if (!FindExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
    {
        SH_ERROR_FORMAT("{} not found!", VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        return;
    }

    validationEnables.push_back(VkValidationFeatureEnableEXT::VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT);

    validationFeatures.sType = VkStructureType::VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
    validationFeatures.enabledValidationFeatureCount = validationEnables.size();
    validationFeatures.pEnabledValidationFeatures = validationEnables.data();
    validationFeatures.pNext = &debugInfo;

    debugInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugInfo.messageSeverity =
        VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugInfo.messageType =
        VkDebugUtilsMessageTypeFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VkDebugUtilsMessageTypeFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VkDebugUtilsMessageTypeFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugInfo.pfnUserCallback = DebugCallback;
    debugInfo.pUserData = nullptr;
}

void VulkanContext::PrepareSurfaceExtension()
{
    if (!FindExtension(VK_KHR_SURFACE_EXTENSION_NAME))
        throw std::runtime_error{ std::format("{} not found", VK_KHR_SURFACE_EXTENSION_NAME) };

    reqExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

    if (!FindExtension(VK_KHR_WIN32_SURFACE_EXTENSION_NAME))
        throw std::runtime_error{ std::format("{} not found", VK_KHR_WIN32_SURFACE_EXTENSION_NAME) };

    reqExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
}

void VulkanContext::CreateInstance()
{
    VkApplicationInfo appInfo{};
    appInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Graphics";
    appInfo.applicationVersion = 1;
    appInfo.pEngineName = "Graphics";
    appInfo.engineVersion = 1;
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo instanceInfo = {};
    instanceInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.enabledLayerCount = reqLayers.size();
    instanceInfo.ppEnabledLayerNames = reqLayers.data();
    instanceInfo.enabledExtensionCount = reqExtensions.size();
    instanceInfo.ppEnabledExtensionNames = reqExtensions.data();
    instanceInfo.pNext = &validationFeatures;

    VkResult result = vkCreateInstance(&instanceInfo, nullptr, &instancePtr);
    assert(result == VkResult::VK_SUCCESS);
    if (result != VkResult::VK_SUCCESS)
        throw std::runtime_error{ "Failed to vkCreateInstance" };
}

void VulkanContext::InitDebugMessenger()
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instancePtr, "vkCreateDebugUtilsMessengerEXT");
    if (func == nullptr)
    {
        SH_ERROR("vkCreateDebugUtilsMessengerEXT is nullptr!");
        bEnableValidationLayers = false;
        return;
    }

    VkResult result = func(instancePtr, &debugInfo, nullptr, &debugMessenger);
    assert(result == VkResult::VK_SUCCESS);
}

void VulkanContext::CreateSurface()
{
    VkWin32SurfaceCreateInfoKHR ci{};
    ci.sType = VkStructureType::VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    ci.hinstance = window.GetInstance();
    ci.hwnd = window.GetHWND();
    
    VK_RESULT_CHECK(vkCreateWin32SurfaceKHR(instancePtr, &ci, nullptr, &surface));
}

void VulkanContext::QueryPhysicalDevice()
{
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instancePtr, &count, nullptr);

    if (count == 0)
    {
        SH_ERROR("GPU count is 0!");
        throw std::runtime_error{ "GPU count is 0!" };
    }
    std::vector<VkPhysicalDevice> gpus(count, VK_NULL_HANDLE);
    vkEnumeratePhysicalDevices(instancePtr, &count, gpus.data());

    for (VkPhysicalDevice gpu : gpus)
    {
        vkGetPhysicalDeviceProperties(gpu, &gpuProps);
        vkGetPhysicalDeviceFeatures(gpu, &gpuFeatures);
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &surfaceCapabilities);

        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &formatCount, nullptr);
        if (formatCount == 0)
            continue;
        surfaceFormats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &formatCount, surfaceFormats.data());

        uint32_t presentCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &presentCount, nullptr);
        if (presentCount == 0)
            continue;
        presentModes.resize(presentCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &presentCount, presentModes.data());

        if (gpuProps.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
            gpuProps.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_CPU)
        {
            vkGetPhysicalDeviceMemoryProperties(gpu, &gpuMemProps);
            this->gpu = gpu;
            return;
        }
    }
}

void VulkanContext::SelectQueue()
{
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, nullptr);
    std::vector<VkQueueFamilyProperties> queueProps(count);
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, queueProps.data());

    for (int i = 0; i < queueProps.size(); ++i)
    {
        if (graphicsQueueFamily != 0xFFFFFFFF && presentQueueFamily != 0xFFFFFFFF && computeQueueFamily != 0xFFFFFFFF) // 다 고른 상태
            break;

        const auto& p = queueProps[i];
        if (p.queueCount == 0)
            continue;

        if (graphicsQueueFamily == 0xFFFFFFFF)
        {
            if (p.queueFlags & VkQueueFlagBits::VK_QUEUE_GRAPHICS_BIT)
                graphicsQueueFamily = i;
        }
        
        if (presentQueueFamily == 0xFFFFFFFF)
        {
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, surface, &presentSupport);
            if (presentSupport)
                presentQueueFamily = i;
        }

        if (computeQueueFamily = 0xFFFFFFFF)
        {
            if (p.queueFlags & VkQueueFlagBits::VK_QUEUE_COMPUTE_BIT)
                computeQueueFamily = i;
        }
    }
    if (graphicsQueueFamily == 0xFFFFFFFF)
    {
        SH_ERROR("Not found graphicsQueueFamily!");
        throw std::runtime_error{ "Not found graphicsQueueFamily!" };
    }
    if (presentQueueFamily == 0xFFFFFFFF)
    {
        SH_ERROR("Not found presentQueueFamily!");
        throw std::runtime_error{ "Not found presentQueueFamily!" };
    }
    if (computeQueueFamily == 0xFFFFFFFF)
    {
        SH_ERROR("Not found computeQueueFamily!");
        throw std::runtime_error{ "Not found computeQueueFamily!" };
    }
}

void VulkanContext::CreateDevice()
{
    float priorities = 1.f;

    std::set<uint32_t> uniqueQueueFamilies = { graphicsQueueFamily, presentQueueFamily };
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    for (uint32_t queueFamily : uniqueQueueFamilies) 
    {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &priorities;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = true;

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicFeatures{};
    dynamicFeatures.sType = VkStructureType::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
    dynamicFeatures.dynamicRendering = true;
    dynamicFeatures.pNext = nullptr;

    VkDeviceCreateInfo ci{};
    ci.sType = VkStructureType::VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.pQueueCreateInfos = queueCreateInfos.data();
    ci.queueCreateInfoCount = queueCreateInfos.size();
    ci.pEnabledFeatures = &deviceFeatures;
    ci.enabledLayerCount = 0;
    ci.ppEnabledLayerNames = nullptr;
    ci.enabledExtensionCount = reqDeviceExtensions.size();
    ci.ppEnabledExtensionNames = reqDeviceExtensions.data();
    ci.pNext = &dynamicFeatures;

    VkResult result = vkCreateDevice(gpu, &ci, nullptr, &device);
    assert(result == VkResult::VK_SUCCESS);
    if (result != VkResult::VK_SUCCESS)
    {
        SH_ERROR_FORMAT("Failed to vkCreateDevice!: {}", string_VkResult(result));
        throw std::runtime_error{ "Failed to vkCreateDevice!" };
    }

    vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);
    vkGetDeviceQueue(device, presentQueueFamily, 0, &presentQueue);
    vkGetDeviceQueue(device, computeQueueFamily, 0, &computeQueue);
}

void VulkanContext::CreateSwapChain()
{
    VkSwapchainKHR oldSwapchain = swapChain;

    VkSurfaceFormatKHR selectFormat = surfaceFormats[0];
    for (const VkSurfaceFormatKHR& format : surfaceFormats)
    {
        if (format.format == VkFormat::VK_FORMAT_R8G8B8A8_SRGB && format.colorSpace == VkColorSpaceKHR::VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            selectFormat = format;
            break;
        }
    }

    VkPresentModeKHR selectPresentMode = VkPresentModeKHR::VK_PRESENT_MODE_FIFO_KHR;
    for (const VkPresentModeKHR& mode : presentModes)
    {
        //삼중 버퍼링
        if (mode == VkPresentModeKHR::VK_PRESENT_MODE_MAILBOX_KHR)
        {
            SH_INFO("VK_PRESENT_MODE_MAILBOX_KHR");
            selectPresentMode = mode;
            break;
        }
        if (mode == VkPresentModeKHR::VK_PRESENT_MODE_IMMEDIATE_KHR)
        {
            selectPresentMode = mode;
        }
    }
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &surfaceCapabilities);

    const VkExtent2D size = surfaceCapabilities.currentExtent;

    uint32_t swapChainImageCount = surfaceCapabilities.minImageCount + 1;
    if (surfaceCapabilities.maxImageCount > 0 && swapChainImageCount > surfaceCapabilities.maxImageCount)
        swapChainImageCount = surfaceCapabilities.maxImageCount;

    VkSwapchainCreateInfoKHR info{};
    info.sType = VkStructureType::VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = surface;
    info.minImageCount = swapChainImageCount;
    info.imageFormat = selectFormat.format;
    info.imageColorSpace = selectFormat.colorSpace;
    info.imageExtent = size;
    info.imageArrayLayers = 1;
    info.imageUsage = VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.preTransform = surfaceCapabilities.currentTransform;
    info.compositeAlpha = VkCompositeAlphaFlagBitsKHR::VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = selectPresentMode;
    info.clipped = true;
    info.oldSwapchain = oldSwapchain;
    if (graphicsQueueFamily == presentQueueFamily)
    {
        info.imageSharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE;
        info.queueFamilyIndexCount = 0;
        info.pQueueFamilyIndices = nullptr;
    }
    else
    {
        uint32_t fams[] = { graphicsQueueFamily, presentQueueFamily };
        info.imageSharingMode = VkSharingMode::VK_SHARING_MODE_CONCURRENT;
        info.queueFamilyIndexCount = 2;
        info.pQueueFamilyIndices = fams;
    }
    VK_RESULT_CHECK(vkCreateSwapchainKHR(device, &info, nullptr, &swapChain));

    uint32_t imageCount = 0;
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

    swapChainImageFormat = selectFormat.format;
    swapChainExtent = size;

    swapChainImageViews.resize(swapChainImages.size());
    for (size_t i = 0; i < swapChainImages.size(); i++) 
    {
        VkImageViewCreateInfo ci{};
        ci.sType = VkStructureType::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ci.image = swapChainImages[i];
        ci.viewType = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D;
        ci.format = swapChainImageFormat;
        ci.components.r = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY;
        ci.components.g = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY;
        ci.components.b = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY;
        ci.components.a = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY;
        ci.subresourceRange.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
        ci.subresourceRange.baseMipLevel = 0;
        ci.subresourceRange.levelCount = 1;
        ci.subresourceRange.baseArrayLayer = 0;
        ci.subresourceRange.layerCount = 1;
        VK_RESULT_CHECK(vkCreateImageView(device, &ci, nullptr, &swapChainImageViews[i]));
    }

    swapChainDepthImages.resize(swapChainImages.size());
    swapChainDepthMemories.resize(swapChainImages.size());
    swapChainDepthImageViews.resize(swapChainImages.size());
    // 깊이 버퍼
    for (std::size_t i = 0; i < swapChainDepthImages.size(); ++i)
    {
        VkImage& depthImg = swapChainDepthImages[i];
        VkImageView& depthView = swapChainDepthImageViews[i];
        VkDeviceMemory& depthMem = swapChainDepthMemories[i];

        VkImageCreateInfo imageCi{};
        imageCi.sType = VkStructureType::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCi.imageType = VkImageType::VK_IMAGE_TYPE_2D;
        imageCi.extent.width = swapChainExtent.width;
        imageCi.extent.height = swapChainExtent.height;
        imageCi.extent.depth = 1;
        imageCi.mipLevels = 1;
        imageCi.arrayLayers = 1;
        imageCi.format = VkFormat::VK_FORMAT_D24_UNORM_S8_UINT;
        imageCi.tiling = VkImageTiling::VK_IMAGE_TILING_OPTIMAL;
        imageCi.initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED;
        imageCi.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageCi.sharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE;
        imageCi.samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
        imageCi.flags = 0;
        VK_RESULT_CHECK(vkCreateImage(device, &imageCi, nullptr, &depthImg));

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, depthImg, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VK_RESULT_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &depthMem));
        vkBindImageMemory(device, depthImg, depthMem, 0);

        VkImageViewCreateInfo ci{};
        ci.sType = VkStructureType::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ci.image = depthImg;
        ci.viewType = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D;
        ci.format = VkFormat::VK_FORMAT_D24_UNORM_S8_UINT;
        ci.components.r = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY;
        ci.components.g = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY;
        ci.components.b = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY;
        ci.components.a = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY;
        ci.subresourceRange.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT;
        ci.subresourceRange.baseMipLevel = 0;
        ci.subresourceRange.levelCount = 1;
        ci.subresourceRange.baseArrayLayer = 0;
        ci.subresourceRange.layerCount = 1;
        VK_RESULT_CHECK(vkCreateImageView(device, &ci, nullptr, &depthView));
    }

}

void VulkanContext::CreateCommandPool()
{
    VkCommandPoolCreateInfo ci{};
    ci.sType = VkStructureType::VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    ci.queueFamilyIndex = graphicsQueueFamily;
    ci.flags = VkCommandPoolCreateFlagBits::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VK_RESULT_CHECK(vkCreateCommandPool(device, &ci, nullptr, &graphicsCommandPool));

    if (graphicsQueueFamily == computeQueueFamily)
        computeCommandPool = graphicsCommandPool;
    else
    {
        ci.queueFamilyIndex = computeQueueFamily;
        VK_RESULT_CHECK(vkCreateCommandPool(device, &ci, nullptr, &computeCommandPool));
    }
}

void VulkanContext::ClearSwapChain()
{
    for (VkImageView view : swapChainDepthImageViews)
        vkDestroyImageView(device, view, nullptr);
    swapChainDepthImageViews.clear();
    for (VkImageView view : swapChainImageViews)
        vkDestroyImageView(device, view, nullptr);
    swapChainImageViews.clear();
    for (VkImage img : swapChainDepthImages)
        vkDestroyImage(device, img, nullptr);
    swapChainDepthImages.clear();
    swapChainImages.clear();
    for (VkDeviceMemory mem : swapChainDepthMemories)
        vkFreeMemory(device, mem, nullptr);
    swapChainDepthMemories.clear();

    vkDestroySwapchainKHR(device, swapChain, nullptr);
    swapChain = VK_NULL_HANDLE;
}