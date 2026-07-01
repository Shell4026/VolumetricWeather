#include "ImGUI.h"
#include "Window.h"
#include "VulkanContext.h"

#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_vulkan.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
	void check_vk_result(VkResult err)
	{
		if (err == 0)
			return;
		fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
		if (err < 0)
			abort();
	}
}

ImGUI::ImGUI(Window& window, VulkanContext& ctx) :
	window(window), ctx(ctx)
{
}

ImGUI::~ImGUI()
{
	if (bInit)
		Clear();
}

void ImGUI::Init()
{
	ImGui::CreateContext();
	ImGui_ImplWin32_Init(window.GetHWND());

	ImGui_ImplVulkan_InitInfo initInfo{};
	initInfo.Instance = ctx.GetInstance();
	initInfo.PhysicalDevice = ctx.GetPhysicalDevice();
	initInfo.Device = ctx.GetDevice();
	initInfo.QueueFamily = ctx.GetGraphicsQueueFamily();
	initInfo.Queue = ctx.GetGraphicsQueue();
	initInfo.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE;
	initInfo.MinImageCount = 2;
	initInfo.ImageCount = 2;
	initInfo.PipelineCache = nullptr;
	initInfo.CheckVkResultFn = check_vk_result;
	initInfo.UseDynamicRendering = true;
	initInfo.PipelineInfoMain.MSAASamples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
	initInfo.PipelineInfoMain.Subpass = 0;
	
	VkFormat colorFormat = ctx.GetSwapChainImagesFormat();;

	VkPipelineRenderingCreateInfo ci{};
	ci.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
	ci.colorAttachmentCount = 1;
	ci.pColorAttachmentFormats = &colorFormat;
	ci.depthAttachmentFormat = ctx.GetSwapChainDepthFormat();
	ci.stencilAttachmentFormat = ctx.GetSwapChainDepthFormat();

	initInfo.PipelineInfoMain.PipelineRenderingCreateInfo = ci;
	ImGui_ImplVulkan_Init(&initInfo);
	//ImGui_ImplVulkan_CreateFontsTexture();

	bInit = true;
}
void ImGUI::Clear()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	bInit = false;
}

void ImGUI::Begin(double dt)
{
	this->dt = dt;

	ImGuiIO& io = ImGui::GetIO();
	io.DeltaTime = static_cast<float>(dt);

	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}
void ImGUI::End()
{
	ImGui::Render();
}

void ImGUI::ProcessEvent(UINT msg, WPARAM wParam, LPARAM lParam)
{
	ImGui_ImplWin32_WndProcHandler(window.GetHWND(), msg, wParam, lParam);
}
