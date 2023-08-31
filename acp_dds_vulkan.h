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

	struct dds_data
	{
		VkImageCreateInfo image_create_info{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		size_t width{ 0 };
		size_t height{ 0 };
		size_t num_bytes{ 0 };
		size_t row_bytes{ 0 };
		size_t num_rows{ 0 };
		image_mip_data image_mip_data[16] = {};
		size_t num_mips{ 0 };
		unsigned char* dss_buffer_data{ nullptr };
	};

	dds_data dds_data_from_memory(void* data, size_t data_size);
	dds_data dds_data_from_file(const char* path);
	void dds_data_free(dds_data* dds_data);

};