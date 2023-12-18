#pragma once
#include <vulkan/vulkan.h>

namespace acp_vulkan
{
	struct image_mip_data
	{
		VkExtent3D extents{};
		uint8_t* data{ nullptr };
		size_t data_size{ 0 };
	};

	constexpr size_t MAX_NUMBER_OF_MIPS = 32;
	struct dds_data
	{
		VkImageCreateInfo image_create_info{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		size_t width{ 0 };
		size_t height{ 0 };
		size_t num_bytes{ 0 };
		size_t row_bytes{ 0 };
		size_t num_rows{ 0 };
		image_mip_data image_mip_data[MAX_NUMBER_OF_MIPS + 1] = {};
		size_t num_mips{ 0 };
		unsigned char* dss_buffer_data{ nullptr };
		void* full_data{ nullptr };
	};

	dds_data dds_data_from_memory(void* data, size_t data_size, bool will_own_data, VkAllocationCallbacks* host_allocator);
	dds_data dds_data_from_file(const char* path, VkAllocationCallbacks* host_allocator);
	void dds_data_free(dds_data* dds_data, VkAllocationCallbacks* host_allocator);

};