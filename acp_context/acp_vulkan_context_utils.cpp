#include <acp_context/acp_vulkan_context.h>
#include <acp_context/acp_vulkan_context_utils.h>
#include <vma/vk_mem_alloc.h>

VkCommandPool acp_vulkan::commands_pool_crate(renderer_context* renderer_context)
{
	VkCommandPoolCreateInfo vkCommandPoolCreateInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	vkCommandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	vkCommandPoolCreateInfo.queueFamilyIndex = renderer_context->graphics_family_index;

	VkCommandPool out = VK_NULL_HANDLE;
	ACP_VK_CHECK(vkCreateCommandPool(renderer_context->logical_device, &vkCommandPoolCreateInfo, renderer_context->host_allocator, &out), renderer_context);
	return out;
}

void acp_vulkan::commands_pool_destroy(renderer_context* renderer_context, VkCommandPool commands_pool)
{
	if (commands_pool == VK_NULL_HANDLE)
		return;

	vkResetCommandPool(renderer_context->logical_device, commands_pool, 0);

	vkDestroyCommandPool(renderer_context->logical_device, commands_pool, renderer_context->host_allocator);
}

VkSemaphore acp_vulkan::semaphore_create(renderer_context* renderer_context)
{
	VkSemaphoreCreateInfo createInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

	VkSemaphore semaphore = 0;
	ACP_VK_CHECK(vkCreateSemaphore(renderer_context->logical_device, &createInfo, 0, &semaphore), renderer_context);

	return semaphore;
}

void acp_vulkan::semaphore_destroy(renderer_context* renderer_context, VkSemaphore semaphore)
{
	if (semaphore == VK_NULL_HANDLE)
		return;

	vkDestroySemaphore(renderer_context->logical_device, semaphore, renderer_context->host_allocator);
}

VkDescriptorPool acp_vulkan::descriptor_pool_create(renderer_context* renderer_context, uint32_t max_descriptor_count)
{
	VkDescriptorPoolCreateInfo info{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };

	VkDescriptorPoolSize pool_sizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_SAMPLER, max_descriptor_count },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, max_descriptor_count },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, max_descriptor_count },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, max_descriptor_count },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, max_descriptor_count },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, max_descriptor_count },
	};
	info.maxSets = max_descriptor_count;

	info.poolSizeCount = sizeof(pool_sizes) / sizeof(pool_sizes[0]);
	info.pPoolSizes = pool_sizes;

	VkDescriptorPool out = VK_NULL_HANDLE;
	ACP_VK_CHECK(vkCreateDescriptorPool(renderer_context->logical_device, &info, renderer_context->host_allocator, &out), renderer_context);
	return out;
}

void acp_vulkan::descriptor_pool_destroy(acp_vulkan::renderer_context* renderer_context, VkDescriptorPool descriptor_pool)
{
	if (descriptor_pool == VK_NULL_HANDLE)
		return;

	vkDestroyDescriptorPool(renderer_context->logical_device, descriptor_pool, renderer_context->host_allocator);
}

VkImageView acp_vulkan::image_view_create(renderer_context* renderer_context, VkImage image, VkFormat format, uint32_t mip_level, uint32_t level_count, VkImageAspectFlags aspectMask)
{
	VkImageViewCreateInfo createInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	createInfo.image = image;
	createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	createInfo.format = format;
	createInfo.subresourceRange.aspectMask = aspectMask;
	createInfo.subresourceRange.baseMipLevel = mip_level;
	createInfo.subresourceRange.levelCount = level_count;
	createInfo.subresourceRange.layerCount = 1;

	VkImageView view = 0;
	ACP_VK_CHECK(vkCreateImageView(renderer_context->logical_device, &createInfo, renderer_context->host_allocator, &view), renderer_context);

	return view;
}

void acp_vulkan::image_view_destroy(renderer_context* renderer_context, VkImageView image_view)
{
	vkDestroyImageView(renderer_context->logical_device, image_view, renderer_context->host_allocator);
}

acp_vulkan::image_data acp_vulkan::image_create(renderer_context* renderer_context, uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, VkImageUsageFlags usage, VmaMemoryUsage memory_usage)
{
	VkImageCreateInfo info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };

	info.imageType = VK_IMAGE_TYPE_2D;
	info.format = format;
	info.extent = { width, height, 1 };
	info.mipLevels = mipLevels;
	info.arrayLayers = 1;
	info.samples = VK_SAMPLE_COUNT_1_BIT;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.usage = usage;
	info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	image_data image = { VK_NULL_HANDLE, nullptr };

	VmaAllocationCreateInfo allocation_info{};
	allocation_info.flags = memory_usage;
	allocation_info.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	ACP_VK_CHECK(vmaCreateImage(renderer_context->gpu_allocator, &info, &allocation_info, &image.image, &image.memory_allocation, nullptr),renderer_context);
	
	return image;
}

void acp_vulkan::image_destroy(renderer_context* renderer_context, image_data image_data)
{
	vmaDestroyImage(renderer_context->gpu_allocator, image_data.image, image_data.memory_allocation);
}

VkFence acp_vulkan::fence_create(renderer_context* renderer_context, bool create_signaled)
{
	VkFenceCreateInfo info{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	if (create_signaled)
		info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkFence out = VK_NULL_HANDLE;
	ACP_VK_CHECK(vkCreateFence(renderer_context->logical_device, &info, renderer_context->host_allocator, &out), renderer_context);

	return out;
}

void acp_vulkan::fence_destroy(renderer_context* renderer_context, VkFence fence)
{
	vkDestroyFence(renderer_context->logical_device, fence, renderer_context->host_allocator);
}

VkSampler acp_vulkan::create_linear_sampler(renderer_context* renderer_context)
{
	VkSamplerCreateInfo sampler_info = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	sampler_info.magFilter = VK_FILTER_LINEAR;
	sampler_info.minFilter = VK_FILTER_LINEAR;
	sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler_info.minFilter = VK_FILTER_LINEAR;
	sampler_info.mipLodBias = 0.0f;
	sampler_info.minLod = 0;
	sampler_info.maxLod = 0;
	sampler_info.anisotropyEnable = VK_FALSE;
	sampler_info.compareEnable = VK_FALSE;
	sampler_info.unnormalizedCoordinates = VK_FALSE;
	VkSampler sampler = VK_NULL_HANDLE;
	ACP_VK_CHECK(vkCreateSampler(renderer_context->logical_device, &sampler_info, renderer_context->host_allocator, &sampler), renderer_context);
	return sampler;
}

void acp_vulkan::destroy_sampler(renderer_context* renderer_context, VkSampler sampler)
{
	vkDestroySampler(renderer_context->logical_device, sampler, renderer_context->host_allocator);
}

VkImageMemoryBarrier2 acp_vulkan::image_barrier(VkImage image, VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 srcAccessMask, VkImageLayout oldLayout, VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask, VkImageLayout newLayout, VkImageAspectFlags aspectMask, uint32_t baseMipLevel, uint32_t levelCount)
{
	VkImageMemoryBarrier2 result = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };

	result.srcStageMask = srcStageMask;
	result.srcAccessMask = srcAccessMask;
	result.dstStageMask = dstStageMask;
	result.dstAccessMask = dstAccessMask;
	result.oldLayout = oldLayout;
	result.newLayout = newLayout;
	result.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	result.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	result.image = image;
	result.subresourceRange.aspectMask = aspectMask;
	result.subresourceRange.baseMipLevel = baseMipLevel;
	result.subresourceRange.levelCount = levelCount;
	result.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	return result;
}

void acp_vulkan::push_pipeline_barrier(VkCommandBuffer commandBuffer, VkDependencyFlags dependencyFlags, size_t bufferBarrierCount, const VkBufferMemoryBarrier2* bufferBarriers, size_t imageBarrierCount, const VkImageMemoryBarrier2* imageBarriers)
{
	VkDependencyInfo dependencyInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
	dependencyInfo.dependencyFlags = dependencyFlags;
	dependencyInfo.bufferMemoryBarrierCount = unsigned(bufferBarrierCount);
	dependencyInfo.pBufferMemoryBarriers = bufferBarriers;
	dependencyInfo.imageMemoryBarrierCount = unsigned(imageBarrierCount);
	dependencyInfo.pImageMemoryBarriers = imageBarriers;

	vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
}

//alex(todo): this is suboptimal as fuck but it will do for now, should upload data properly.
acp_vulkan::buffer_data acp_vulkan::upload_data(renderer_context* context, void* verts, uint32_t num_vertices, uint32_t one_vertex_size, VkBufferUsageFlagBits usage)
{
	buffer_data staging_buffer{};
	{
		VkBufferCreateInfo buffer_info = {};
		buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer_info.size = num_vertices * one_vertex_size;
		buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

		VmaAllocationCreateInfo vmaalloc_info = {};
		vmaalloc_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;

		ACP_VK_CHECK(vmaCreateBuffer(context->gpu_allocator, &buffer_info, &vmaalloc_info,
			&staging_buffer.buffer,
			&staging_buffer.allocation,
			nullptr), context);
	}

	void* data = nullptr;
	vmaMapMemory(context->gpu_allocator, staging_buffer.allocation, &data);
	{
		memcpy(data, verts, num_vertices * one_vertex_size);
	}
	vmaUnmapMemory(context->gpu_allocator, staging_buffer.allocation);

	//mesh on gpu buffer
	buffer_data out;
	{
		VkBufferCreateInfo buffer_info = {};
		buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer_info.size = num_vertices * one_vertex_size;
		buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage;

		VmaAllocationCreateInfo vmaalloc_info = {};
		vmaalloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

		ACP_VK_CHECK(vmaCreateBuffer(context->gpu_allocator, &buffer_info, &vmaalloc_info,
			&out.buffer,
			&out.allocation,
			nullptr), context);
	}

	immediate_submit(context, [&staging_buffer, out, num_vertices, one_vertex_size](VkCommandBuffer cmd) {
		VkBufferCopy copy{};
		copy.dstOffset = 0;
		copy.srcOffset = 0;
		copy.size = num_vertices * one_vertex_size;
		vkCmdCopyBuffer(cmd, staging_buffer.buffer, out.buffer, 1, &copy);
		}
	);

	vmaDestroyBuffer(context->gpu_allocator, staging_buffer.buffer, staging_buffer.allocation);

	return out;
}

acp_vulkan::image_data acp_vulkan::upload_image(renderer_context* context, image_mip_data* image_mip_data, const VkImageCreateInfo& image_info)
{
	size_t total_size = 0;
	for (size_t ii = 0; ii < image_info.mipLevels; ++ii)
		total_size += image_mip_data[ii].data_size;

	buffer_data staging_buffer{};
	{
		VkBufferCreateInfo buffer_info = {};
		buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer_info.size = total_size;
		buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

		VmaAllocationCreateInfo vmaalloc_info = {};
		vmaalloc_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;

		ACP_VK_CHECK(vmaCreateBuffer(context->gpu_allocator, &buffer_info, &vmaalloc_info,
			&staging_buffer.buffer,
			&staging_buffer.allocation,
			nullptr), context);
	}

	{
		void* stageing_data = nullptr;
		vmaMapMemory(context->gpu_allocator, staging_buffer.allocation, &stageing_data);
		size_t byes_written = 0;
		{
			for (size_t ii = 0; ii < image_info.mipLevels; ++ii)
			{
				memcpy(reinterpret_cast<uint8_t*>(stageing_data) + byes_written, image_mip_data[ii].data, image_mip_data[ii].data_size);
				byes_written += image_mip_data[ii].data_size;
			}
		}
		vmaUnmapMemory(context->gpu_allocator, staging_buffer.allocation);
	}

	// allocate new image on the gpu
	image_data new_image{};
	{
		VmaAllocationCreateInfo img_alloc_info = {};
		img_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

		vmaCreateImage(context->gpu_allocator, &image_info, &img_alloc_info, &new_image.image, &new_image.memory_allocation, nullptr);
	}

	immediate_submit(context,[&new_image, image_mip_data, &staging_buffer, image_info](VkCommandBuffer cmd) {
		VkImageSubresourceRange range{};
		range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		range.levelCount = image_info.mipLevels;
		range.layerCount = image_info.arrayLayers;

		//transition the new image to a linear layout
		{
			VkImageMemoryBarrier image_barrier_to_transfer = {};
			image_barrier_to_transfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;

			image_barrier_to_transfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			image_barrier_to_transfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			image_barrier_to_transfer.image = new_image.image;
			image_barrier_to_transfer.subresourceRange = range;

			image_barrier_to_transfer.srcAccessMask = 0;
			image_barrier_to_transfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &image_barrier_to_transfer);
		}

		//copy the data from the cpu to the gpu
		std::vector<VkBufferImageCopy> copyRegions;
		VkDeviceSize buffer_read_offset = 0;
		for (uint32_t mip = 0; mip < image_info.mipLevels; ++mip)
		{
			VkBufferImageCopy copyRegion = {};
			copyRegion.bufferOffset = buffer_read_offset;
			buffer_read_offset += image_mip_data[mip].data_size;
			copyRegion.bufferRowLength = 0;
			copyRegion.bufferImageHeight = 0;

			copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyRegion.imageSubresource.mipLevel = mip;
			copyRegion.imageSubresource.baseArrayLayer = 0;
			copyRegion.imageSubresource.layerCount = image_info.arrayLayers;
			copyRegion.imageExtent = image_mip_data[mip].extents;

			copyRegions.push_back(copyRegion);
		}
		vkCmdCopyBufferToImage(cmd, staging_buffer.buffer, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, uint32_t(copyRegions.size()), copyRegions.data());

		//transition the new image to the optimal layout
		{
			VkImageMemoryBarrier image_barrier_to_readable = {};
			image_barrier_to_readable.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			image_barrier_to_readable.image = new_image.image;
			image_barrier_to_readable.subresourceRange = range;
			image_barrier_to_readable.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			image_barrier_to_readable.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			image_barrier_to_readable.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			image_barrier_to_readable.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &image_barrier_to_readable);
		}
		});

	vmaDestroyBuffer(context->gpu_allocator, staging_buffer.buffer, staging_buffer.allocation);

	return new_image;
}