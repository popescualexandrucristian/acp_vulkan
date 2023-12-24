#pragma once
#include <vulkan/vulkan.h>

namespace acp_vulkan
{
	struct debug_color
	{
		float r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f;
	};

	void debug_init(VkDevice device);

	void debug_set_object_name(VkDevice device, void* object, VkObjectType object_type, const char* name);

	void debug_set_object_tag(VkDevice device, void* object, VkObjectType object_type, uint64_t name, size_t tag_size, const void* tag);

	void debug_begin_region(VkQueue queue, const char* marker_name, debug_color color);

	void debug_end_region(VkQueue queue);

	void debug_insert(VkQueue queue, const char* marker_name, debug_color color);

	void debug_begin_region(VkCommandBuffer command_buffer, const char* marker_name, debug_color color);

	void debug_end_region(VkCommandBuffer command_buffer);

	void debug_insert(VkCommandBuffer command_buffer, const char* marker_name, debug_color color);
};