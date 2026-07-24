#include "TextureLoader.h"

#include "core/Util.h"

#include "render/VulkanContext.h"

#include "stb_image.h"
auto TextureLoader::Load(const VulkanContext& ctx, const std::filesystem::path& filePath) -> std::optional<VulkanImage>
{
	const std::vector<uint8_t> binary = util::LoadBinary(filePath);
	if (binary.empty())
		return {};

	int width, height, channel;
	int info = stbi_info_from_memory(binary.data(), binary.size(), & width, &height, &channel);
	if (!info)
		return {};

	stbi_uc* pixels = stbi_load_from_memory(binary.data(), binary.size(), & width, &height, &channel, STBI_rgb_alpha);

	VkImageCreateInfo ci = VulkanImage::GetCreateInfo();
	ci.extent.width = width;
	ci.extent.height = height;
	ci.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	VulkanImage img{ctx, ci, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT};
	img.SetData(pixels);
	stbi_image_free(pixels);
	return img;
}