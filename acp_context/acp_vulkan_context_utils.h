#pragma once

#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>
#include <acp_dds_vulkan.h>
#include <functional>

#define ACP_VK_CHECK(x, c)										\
do																\
{																\
	VkResult err = x;											\
	if (err)													\
	{															\
		VkDebugUtilsMessengerCallbackDataEXT debug_data{};		\
		char result_storage[128] = {0};							\
		sprintf(result_storage, "vulkan error : %d\n", x);		\
		debug_data.pMessage = result_storage;					\
		c->debug_callback(VK_DEBUG_REPORT_ERROR_BIT_EXT,		\
			VK_DEBUG_REPORT_OBJECT_TYPE_UNKNOWN_EXT,			\
				0, 0, x, nullptr, result_storage, nullptr);		\
	}															\
} while (0)

namespace acp_vulkan
{	
	struct renderer_context;

	VkCommandPool commands_pool_crate(acp_vulkan::renderer_context* renderer_context);
	void commands_pool_destroy(acp_vulkan::renderer_context* renderer_context, VkCommandPool commands_pool);

	VkSemaphore semaphore_create(acp_vulkan::renderer_context* renderer_context);
	void semaphore_destroy(acp_vulkan::renderer_context* renderer_context, VkSemaphore semaphore);

	VkDescriptorPool descriptor_pool_create(acp_vulkan::renderer_context* renderer_context, uint32_t max_descriptor_count);
	void descriptor_pool_destroy(acp_vulkan::renderer_context* renderer_context, VkDescriptorPool descriptor_pool);

	VkImageView image_view_create(acp_vulkan::renderer_context* renderer_context, VkImage image, VkFormat format, uint32_t mip_level, uint32_t level_count, VkImageAspectFlags aspectMask);
	void image_view_destroy(acp_vulkan::renderer_context* renderer_context, VkImageView image_view);

	struct image_data
	{
		VkImage image{ VK_NULL_HANDLE };
		VmaAllocation memory_allocation{ nullptr };
	};
	image_data image_create(acp_vulkan::renderer_context* renderer_context, uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, VkImageUsageFlags usage, VmaMemoryUsage memory_usage);
	void image_destroy(acp_vulkan::renderer_context* renderer_context, image_data image_data);

	VkFence fence_create(acp_vulkan::renderer_context* renderer_context, bool create_signaled);
	void fence_destroy(acp_vulkan::renderer_context* renderer_context, VkFence fence);

	VkSampler create_linear_sampler(renderer_context* renderer_context);
	void destroy_sampler(renderer_context* renderer_context, VkSampler sampler);

	VkImageMemoryBarrier2 image_barrier(VkImage image, VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 srcAccessMask, VkImageLayout oldLayout, VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask, VkImageLayout newLayout, VkImageAspectFlags aspectMask, uint32_t baseMipLevel, uint32_t levelCount);

	void push_pipeline_barrier(VkCommandBuffer commandBuffer, VkDependencyFlags dependencyFlags, size_t bufferBarrierCount, const VkBufferMemoryBarrier2* bufferBarriers, size_t imageBarrierCount, const VkImageMemoryBarrier2* imageBarriers);

	struct buffer_data {
		VkBuffer buffer{ VK_NULL_HANDLE };
		VmaAllocation allocation{ nullptr };
	};
	buffer_data upload_data(renderer_context* context, void* verts, uint32_t num_vertices, uint32_t one_vertex_size, VkBufferUsageFlagBits usage);
	image_data upload_image(renderer_context* context, image_mip_data* image_mip_data, const VkImageCreateInfo& image_info);
};