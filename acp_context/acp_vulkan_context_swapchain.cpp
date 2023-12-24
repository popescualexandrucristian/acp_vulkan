#include <acp_context/acp_vulkan_context.h>
#include <acp_context/acp_vulkan_context_swapchain.h>
#include <acp_context/acp_vulkan_context_utils.h>
#include <algorithm>
#include <vector>

static VkPresentModeKHR get_present_mode(acp_vulkan::renderer_context* context, bool vsync)
{
	uint32_t num_present_modes = 0;
	ACP_VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(context->physical_device, context->surface, &num_present_modes, nullptr), context);

	std::vector<VkPresentModeKHR> present_modes(num_present_modes);

	ACP_VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(context->physical_device, context->surface, &num_present_modes, present_modes.data()), context);

	for (VkPresentModeKHR present_mode : present_modes)
	{
		if (present_mode == VK_PRESENT_MODE_FIFO_KHR && vsync)
			return VK_PRESENT_MODE_FIFO_KHR;
		else if (present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR && !vsync)
			return VK_PRESENT_MODE_IMMEDIATE_KHR;
	}

	return present_modes[0];
}

static VkSwapchainKHR create_swapchain(acp_vulkan::renderer_context* context, VkSurfaceCapabilitiesKHR surface_caps, uint32_t width, uint32_t height, bool vsync, VkSwapchainKHR old_swapchain)
{
	VkCompositeAlphaFlagBitsKHR surfaceComposite =
		(surface_caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
		? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
		: (surface_caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
		? VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR
		: (surface_caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
		? VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR
		: VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;

	VkSwapchainCreateInfoKHR create_info = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	create_info.surface = context->surface;
	create_info.minImageCount = std::max(3u, surface_caps.minImageCount);
	create_info.imageFormat = context->swapchain_format;
	create_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	create_info.imageExtent.width = width;
	create_info.imageExtent.height = height;
	create_info.imageArrayLayers = 1;
	create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	create_info.queueFamilyIndexCount = 1;
	create_info.pQueueFamilyIndices = &context->graphics_family_index;
	create_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	create_info.compositeAlpha = surfaceComposite;
	create_info.presentMode = get_present_mode(context, vsync);
	create_info.oldSwapchain = old_swapchain;


	VkSwapchainKHR swapchain = 0;
	ACP_VK_CHECK(vkCreateSwapchainKHR(context->logical_device, &create_info, 0, &swapchain), context);

	return swapchain;
}

static void destroy_image_views(acp_vulkan::renderer_context* context, acp_vulkan::swapchain* swapchain)
{
	for (const VkImageView& i : swapchain->views)
		vkDestroyImageView(context->logical_device, i, context->host_allocator);

	for (const VkImageView& i : swapchain->depth_views)
		vkDestroyImageView(context->logical_device, i, context->host_allocator);

	swapchain->views.clear();
	swapchain->depth_views.clear();
}

static void destroy_depth_images(acp_vulkan::renderer_context* context, acp_vulkan::swapchain* swapchain)
{
	for (const acp_vulkan::image_data& i : swapchain->depth_images)
		acp_vulkan::image_destroy(context, i);
}


std::vector<VkImageView> create_image_views(acp_vulkan::renderer_context* context, const std::vector<VkImage>& images)
{

	std::vector<VkImageView> out;
	for (uint32_t i = 0; i < images.size(); ++i)
		out.push_back(image_view_create(context, images[i], context->swapchain_format, 0, 1, VK_IMAGE_ASPECT_COLOR_BIT, "swapchain_image_views"));

	return out;
}

std::vector<VkImageView> create_depth_image_views(acp_vulkan::renderer_context* context, const std::vector<acp_vulkan::image_data>& images)
{
	std::vector<VkImageView> out;
	for (uint32_t i = 0; i < images.size(); ++i)
		out.push_back(image_view_create(context, images[i].image, context->depth_format, 0, 1, VK_IMAGE_ASPECT_DEPTH_BIT, "depth_image_views"));

	return out;
}

std::vector<acp_vulkan::image_data> create_depth_images(acp_vulkan::renderer_context* context, acp_vulkan::swapchain* swapchain, size_t image_count)
{
	std::vector<acp_vulkan::image_data> out;
	for (size_t i = 0; i < image_count; ++i)
		out.push_back(acp_vulkan::image_create(context, swapchain->width, swapchain->height, 1, context->depth_format, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, "depth_images"));

	return out;
}

acp_vulkan::swapchain* acp_vulkan::swapchain_init(renderer_context* context, uint32_t width, uint32_t height, bool use_vsync, bool use_depth)
{
	swapchain* out = new swapchain();

	VkSurfaceCapabilitiesKHR surface_caps{};
	ACP_VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context->physical_device, context->surface, &surface_caps), context);

	out->swapchain = create_swapchain(context, surface_caps, width, height, use_vsync, VK_NULL_HANDLE);
	if (out->swapchain == VK_NULL_HANDLE)
		return nullptr;
	out->width = width;
	out->height = height;
	out->vsync = use_vsync;
	
	uint32_t image_count = 0;
	ACP_VK_CHECK(vkGetSwapchainImagesKHR(context->logical_device, out->swapchain, &image_count, 0), context);

	std::vector<VkImage> images(image_count);
	ACP_VK_CHECK(vkGetSwapchainImagesKHR(context->logical_device, out->swapchain, &image_count, images.data()), context);
	out->images = std::move(images);
	out->views = std::move(create_image_views(context, out->images));

	if (use_depth)
	{
		out->depth_images = std::move(create_depth_images(context, out, out->images.size()));
		out->depth_views = std::move(create_depth_image_views(context, out->depth_images));
	}

	return out;
}

void acp_vulkan::swapchian_destroy(swapchain* swapchain, renderer_context* context)
{
	if (!swapchain)
		return;

	destroy_image_views(context, swapchain);
	destroy_depth_images(context, swapchain);

	if (swapchain->swapchain != VK_NULL_HANDLE)
	{
		ACP_VK_CHECK(vkDeviceWaitIdle(context->logical_device), context);
		vkDestroySwapchainKHR(context->logical_device, swapchain->swapchain, context->host_allocator);
		swapchain->images.clear();
		swapchain->swapchain = VK_NULL_HANDLE;
	}
	delete swapchain;
}

bool acp_vulkan::swapchian_update(swapchain* swapchain, renderer_context* context, uint32_t new_width, uint32_t new_height, bool use_vsync, bool use_depth)
{
	if (new_width == 0 || new_height == 0)
		return false;

	if (new_width == swapchain->width && new_height == swapchain->height && swapchain->vsync == use_vsync)
		return true;

	VkSurfaceCapabilitiesKHR surface_caps{};
	ACP_VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context->physical_device, context->surface, &surface_caps), context);

	VkSwapchainKHR old_swapchain = swapchain->swapchain;
	swapchain->swapchain = VK_NULL_HANDLE;
	ACP_VK_CHECK(vkDeviceWaitIdle(context->logical_device), context);

	swapchain->swapchain = create_swapchain(context, surface_caps, new_width, new_height, use_vsync, old_swapchain);
	if (swapchain->swapchain == VK_NULL_HANDLE)
	{
		swapchain->swapchain = old_swapchain;
		return false;
	}

	swapchain->width = new_width;
	swapchain->height = new_height;
	swapchain->vsync = use_vsync;

	uint32_t image_count = 0;
	ACP_VK_CHECK(vkGetSwapchainImagesKHR(context->logical_device, swapchain->swapchain, &image_count, 0), context);

	std::vector<VkImage> images(image_count);
	ACP_VK_CHECK(vkGetSwapchainImagesKHR(context->logical_device, swapchain->swapchain, &image_count, images.data()), context);
	swapchain->images = std::move(images);

	destroy_image_views(context, swapchain);
	destroy_depth_images(context, swapchain);

	swapchain->views = create_image_views(context, swapchain->images);

	if (use_depth)
	{
		swapchain->depth_images = std::move(create_depth_images(context, swapchain, swapchain->images.size()));
		swapchain->depth_views = std::move(create_depth_image_views(context, swapchain->depth_images));
	}

	vkDestroySwapchainKHR(context->logical_device, old_swapchain, context->host_allocator);

	ACP_VK_CHECK(vkDeviceWaitIdle(context->logical_device), context);
	return true;
}

uint32_t acp_vulkan::acquire_next_image(swapchain* swapchain, renderer_context* context, VkFence fence, VkSemaphore semaphore)
{
	VkAcquireNextImageInfoKHR vkAcquireNextImageInfoKHR{ VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR };
	//alex(todo): is this correct, should it be 1 ?
	vkAcquireNextImageInfoKHR.deviceMask = 1;
	vkAcquireNextImageInfoKHR.fence = fence;
	vkAcquireNextImageInfoKHR.semaphore = semaphore;
	vkAcquireNextImageInfoKHR.swapchain = swapchain->swapchain;
	vkAcquireNextImageInfoKHR.timeout = ~0uLL;
	uint32_t out = 0u;
	ACP_VK_CHECK(vkAcquireNextImage2KHR(context->logical_device, &vkAcquireNextImageInfoKHR, &out), context);

	return out;
}